// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/variations/net/variations_command_line.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"
#include "extensions/browser/blob_reader.h"
#include "extensions/browser/extensions_browser_client.h"
#include "net/base/network_change_notifier.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "base/base64.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/feedback_private/proto/hpke.pb.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"
#include "third_party/cros_system_api/dbus/debugd/dbus-constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS)
#include "base/base64.h"
#include "base/feature_list.h"
#endif

namespace extensions {

using system_logs::SysLogsFetcherCallback;
using system_logs::SystemLogsFetcher;
using system_logs::SystemLogsResponse;

namespace {

#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kVariationsStateAttachmentName[] = "variations_state.bin";
#endif

#if BUILDFLAG(IS_CHROMEOS)
// The paths are relative to "/var/log/" by default, which can be overwritten
// for testing purpose.
constexpr base::FilePath::CharType kBluetoothLogsFilePath[] =
    FILE_PATH_LITERAL("bluetooth/log.bz2");
constexpr base::FilePath::CharType kBluetoothLogsFilePathOld[] =
    FILE_PATH_LITERAL("bluetooth/log.bz2.old");
constexpr base::FilePath::CharType kBluetoothQualityReportFilePath[] =
    FILE_PATH_LITERAL("bluetooth/bluetooth_quality_report");

constexpr char kBluetoothLogsAttachmentName[] = "bluetooth_logs.bz2";
constexpr char kBluetoothLogsAttachmentNameOld[] = "bluetooth_logs.old.bz2";
constexpr char kBluetoothQualityReportAttachmentName[] =
    "bluetooth_quality_report";

constexpr char kVariationsAttachmentName[] = "variations.binary";
constexpr char kVariationsFetchHpkeKey[] =
    "https://www.gstatic.com/chromeos-feedback-variations-encryption-key/"
    "public_keyset.json";
constexpr int kVariationsMaxDownloadBytes = 512;

void AddAttachment(scoped_refptr<feedback::FeedbackData> feedback_data,
                   const base::FilePath& root_path,
                   const std::string& file_path,
                   const std::string& attachment_name) {
  std::string temp_log_content;
  if (base::ReadFileToString(root_path.Append(file_path), &temp_log_content)) {
    feedback_data->AddFile(attachment_name, std::move(temp_log_content));
  } else {
    LOG(WARNING) << "failed to add attachment " << attachment_name
                 << ": could not read file: " << file_path << " in "
                 << root_path.value();
  }
}

void AttachBluetoothLogs(scoped_refptr<feedback::FeedbackData> feedback_data,
                         const base::FilePath& root_path) {
  AddAttachment(feedback_data, root_path, kBluetoothLogsFilePath,
                kBluetoothLogsAttachmentName);
  AddAttachment(feedback_data, root_path, kBluetoothLogsFilePathOld,
                kBluetoothLogsAttachmentNameOld);
  AddAttachment(feedback_data, root_path, kBluetoothQualityReportFilePath,
                kBluetoothQualityReportAttachmentName);
}

// A new case must be added for every new log type. Otherwise the code should
// not compile.
std::string_view GetAttachmentName(debugd::FeedbackBinaryLogType log_type) {
  switch (log_type) {
    case debugd::WIFI_FIRMWARE_DUMP:
      return "wifi_firmware_dumps.tar.zst";
    case debugd::BLUETOOTH_FIRMWARE_DUMP:
      return "bluetooth_firmware_dumps.tar.zst";
  }
}
#endif

#if !BUILDFLAG(IS_CHROMEOS)
void IncludeVariations(scoped_refptr<feedback::FeedbackData> feedback_data) {
  std::vector<uint8_t> ciphertext;
  auto status =
      variations::VariationsCommandLine::GetForCurrentProcess().EncryptToString(
          &ciphertext);
  base::UmaHistogramEnumeration("Variations.VariationsStateEncryptionStatus",
                                status);
  // Variations is at best effort.
  if (status == variations::VariationsStateEncryptionStatus::kSuccess) {
    // This is a binary file.
    feedback_data->AddFile(kVariationsStateAttachmentName,
                           std::string(ciphertext.begin(), ciphertext.end()));
  }
}
#endif

void RedactFeedbackData(scoped_refptr<feedback::FeedbackData> feedback_data) {
  redaction::RedactionTool redactor(nullptr);
  redactor.EnableCreditCardRedaction(true);
  feedback_data->RedactDescription(redactor);
}

}  // namespace

FeedbackService::FeedbackService(content::BrowserContext* browser_context)
    : FeedbackService(
          browser_context,
          ExtensionsAPIClient::Get()->GetFeedbackPrivateDelegate()) {}

FeedbackService::FeedbackService(content::BrowserContext* browser_context,
                                 FeedbackPrivateDelegate* delegate)
    : browser_context_(browser_context), delegate_(delegate) {}

FeedbackService::~FeedbackService() = default;

void FeedbackService::RedactThenSendFeedback(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&RedactFeedbackData, feedback_data),
      base::BindOnce(&FeedbackService::SendFeedback, this, params,
                     feedback_data, std::move(callback)));
}

#if BUILDFLAG(IS_CHROMEOS)
void FeedbackService::SetLogFilesRootPathForTesting(
    const base::FilePath& log_file_root) {
  log_file_root_ = log_file_root;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// After the attached file and screenshot if available are fetched, the callback
// will be invoked. Other further processing will be done in background. The
// report will be sent out once all data are in place.
void FeedbackService::SendFeedback(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback) {
  auto* browser_client = ExtensionsBrowserClient::Get();
  feedback_data->set_locale(browser_client->GetApplicationLocale());
  feedback_data->set_user_agent(embedder_support::GetUserAgent());

  FetchAttachedFileAndScreenshot(
      feedback_data,
      base::BindOnce(&FeedbackService::OnAttachedFileAndScreenshotFetched, this,
                     params, feedback_data, std::move(callback)));
}

void FeedbackService::FetchAttachedFileAndScreenshot(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    base::OnceClosure callback) {
  const bool must_attach_file = !feedback_data->attached_file_uuid().empty();
  const bool must_attach_screenshot = !feedback_data->screenshot_uuid().empty();
  auto barrier_closure = base::BarrierClosure(
      (must_attach_file ? 1 : 0) + (must_attach_screenshot ? 1 : 0),
      std::move(callback));

  if (must_attach_file) {
    auto populate_attached_file = base::BindOnce(
        [](scoped_refptr<feedback::FeedbackData> feedback_data,
           std::string data, int64_t /*length*/) {
          feedback_data->set_attached_file_uuid(std::string());
          feedback_data->AttachAndCompressFileData(std::move(data));
        },
        feedback_data);

    BlobReader::Read(
        browser_context_->GetBlobRemote(feedback_data->attached_file_uuid()),
        std::move(populate_attached_file).Then(barrier_closure));
  }

  if (must_attach_screenshot) {
    auto populate_screenshot = base::BindOnce(
        [](scoped_refptr<feedback::FeedbackData> feedback_data,
           std::string data, int64_t /*length*/) {
          feedback_data->set_screenshot_uuid(std::string());
          feedback_data->set_image(std::move(data));
        },
        feedback_data);
    BlobReader::Read(
        browser_context_->GetBlobRemote(feedback_data->screenshot_uuid()),
        std::move(populate_screenshot).Then(barrier_closure));
  }
}

void FeedbackService::OnAttachedFileAndScreenshotFetched(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback) {
  if (params.load_system_info) {
    // The user has chosen to send system logs. They (and on ash more logs)
    // will be loaded in the background without blocking the client.
    FetchSystemInformation(params, feedback_data);
  } else {
#if BUILDFLAG(IS_CHROMEOS)
    if (feedback_data->sys_info()->size() > 0) {
      // The user has chosen to send system logs which has been loaded from the
      // client side. On ash, extra logs need to be fetched.
      FetchExtraLogs(params, feedback_data);
    } else {
      // The user has chosen not to send system logs.
      OnAllLogsFetched(params, feedback_data);
    }
#else
    OnAllLogsFetched(params, feedback_data);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  base::UmaHistogramMediumTimes(
      "Feedback.Duration.FormSubmitToConfirmation",
      base::TimeTicks::Now() - params.form_submit_time);

  // True means report will be sent shortly.
  // False means report will be sent once the device is online.
  const bool status = !net::NetworkChangeNotifier::IsOffline();

  UMA_HISTOGRAM_BOOLEAN("Feedback.ReportSending.Online", status);

  // Notify client that data submitted has been received successfully. The
  // report will be sent out once further processing is done.
  std::move(callback).Run(status);
}

void FeedbackService::FetchSystemInformation(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  base::TimeTicks fetch_start_time = base::TimeTicks::Now();
  delegate_->FetchSystemInformation(
      browser_context_,
      base::BindOnce(&FeedbackService::OnSystemInformationFetched, this,
                     fetch_start_time, params, feedback_data));
}

void FeedbackService::OnSystemInformationFetched(
    base::TimeTicks fetch_start_time,
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  // Fetching is currently slow and could take up to 2 minutes on Chrome OS.
  base::UmaHistogramMediumTimes("Feedback.Duration.FetchSystemInformation",
                                base::TimeTicks::Now() - fetch_start_time);
  if (sys_info) {
    for (auto& itr : *sys_info) {
      if (FeedbackCommon::IncludeInSystemLogs(itr.first,
                                              params.is_internal_email))
        feedback_data->AddLog(std::move(itr.first), std::move(itr.second));
    }
  }
#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(variations::kFeedbackIncludeVariations)) {
    IncludeVariations(feedback_data);
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  FetchExtraLogs(params, feedback_data);
#else
  OnAllLogsFetched(params, feedback_data);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void FeedbackService::FetchExtraLogs(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  delegate_->FetchExtraLogs(
      feedback_data,
      base::BindOnce(&FeedbackService::OnExtraLogsFetched, this, params));
}

void FeedbackService::OnExtraLogsFetched(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  auto barrier_closure =
      base::BarrierClosure((params.send_bluetooth_logs ? 2 : 0) +
                               (params.send_wifi_debug_logs ? 1 : 0) + 1,
                           base::BindOnce(&FeedbackService::OnAllLogsFetched,
                                          this, params, feedback_data));

  EncryptVariations(feedback_data, barrier_closure);

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  const auto account_identifier =
      cryptohome::CreateAccountIdentifierFromAccountId(
          user ? user->GetAccountId() : EmptyAccountId());

  // If bluetooth logs are requested, invoke AttachBluetoothLogs to add
  // them in a separate thread to avoid blocking the UI thread.
  if (params.send_bluetooth_logs) {
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&AttachBluetoothLogs, feedback_data, log_file_root_),
        barrier_closure);

    binary_log_files_reader_.GetFeedbackBinaryLogs(
        account_identifier,
        debugd::FeedbackBinaryLogType::BLUETOOTH_FIRMWARE_DUMP,
        base::BindOnce(&FeedbackService::OnBinaryLogFilesFetched, this, params,
                       feedback_data, barrier_closure));
  }

  if (params.send_wifi_debug_logs) {
    binary_log_files_reader_.GetFeedbackBinaryLogs(
        account_identifier, debugd::FeedbackBinaryLogType::WIFI_FIRMWARE_DUMP,
        base::BindOnce(&FeedbackService::OnBinaryLogFilesFetched, this, params,
                       feedback_data, barrier_closure));
  }
}

void FeedbackService::OnBinaryLogFilesFetched(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    base::RepeatingClosure barrier_closure_callback,
    feedback::BinaryLogFilesReader::BinaryLogsResponse binary_logs_response) {
  if (binary_logs_response) {
    for (auto& item : *binary_logs_response) {
      feedback_data->AddFile(GetAttachmentName(item.first).data(),
                             std::move(item.second));
    }
  }
  std::move(barrier_closure_callback).Run();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void FeedbackService::OnAllLogsFetched(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  if (!params.send_tab_titles) {
    feedback_data->RemoveLog(
        feedback::FeedbackReport::kMemUsageWithTabTitlesKey);
  }
  feedback_data->CompressSystemInfo();

  if (params.send_histograms) {
    std::string histograms =
        base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL);
    feedback_data->SetAndCompressHistograms(std::move(histograms));
  }

  if (params.send_autofill_metadata) {
    feedback_data->CompressAutofillMetadata();
  }

  DCHECK(feedback_data->attached_file_uuid().empty());
  DCHECK(feedback_data->screenshot_uuid().empty());

#if BUILDFLAG(IS_CHROMEOS)
  // Send feedback to Assistant server if triggered from Google Assistant.
  if (feedback_data->from_assistant()) {
    ash::AssistantController::Get()->SendAssistantFeedback(
        feedback_data->assistant_debug_info_allowed(),
        feedback_data->description(), feedback_data->image());
  }
#endif

  // Signal the feedback object that the data from the feedback page has been
  // filled - the object will manage sending of the actual report.
  feedback_data->OnFeedbackPageDataComplete();
  base::UmaHistogramTimes("Feedback.Duration.FormSubmitToSendQueue",
                          base::TimeTicks::Now() - params.form_submit_time);
}

#if BUILDFLAG(IS_CHROMEOS)
void FeedbackService::EncryptVariations(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    base::RepeatingClosure barrier_closure) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "chromeos_feedback_report_hpke_public_key_fetch_for_variations", R"(
        semantics {
          sender: "ChromeOS Feedback Report App"
          description:
            "Users can press Alt+Shift+i to report a bug or a feedback in "
            "general. Here we fetch a Hpke public key to encrypt "
            "the current running variations. This is ChromeOS-only."
          trigger:
            "When user chooses to send feedback to Google."
          data:
            "Fetches a HpKe public key. This key is used to encrypt the "
            "variations that are running at the time the feedback report was "
            "generated, and used by incident management engineering to triage "
            "issues potentially caused by experiments. "
            "If the user unchecks 'Send system information', this will "
            "not be fetched and variations will not be included in the "
            "feedback report."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "cros-feedback-app@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2024-11-06"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings and is only activated "
            "by direct user request."
          chrome_policy {
            UserFeedbackAllowed {
              UserFeedbackAllowed: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kVariationsFetchHpkeKey);
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);

  if (!url_loader_factory_) {
    url_loader_factory_ = browser_context_->GetDefaultStoragePartition()
                              ->GetURLLoaderFactoryForBrowserProcess();
  }

  // Loader will be owned by the callback, so we need a temporary reference to
  // avoid use after move.
  network::SimpleURLLoader* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&FeedbackService::OnVariationsFetchHpkeURL, this,
                     std::move(loader), feedback_data, barrier_closure),
      kVariationsMaxDownloadBytes);
}

void FeedbackService::OnVariationsFetchHpkeURL(
    std::unique_ptr<network::SimpleURLLoader> loader,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    base::RepeatingClosure barrier_closure,
    std::unique_ptr<std::string> hpke_public_key) {
  if (!loader) {
    LOG(ERROR) << "invalid loader";
    return VariationsFinished(false, barrier_closure);
  }

  auto net_error = loader->NetError();
  int http_error = 0;
  if (loader->ResponseInfo() && loader->ResponseInfo()->headers) {
    http_error = loader->ResponseInfo()->headers->response_code();
  }
  if (!hpke_public_key || http_error != net::HTTP_OK) {
    LOG(ERROR) << "Unable to fetch hpke_public_key. http code: " << http_error
               << ", net error: " << net_error;
    return VariationsFinished(false, barrier_closure);
  }
  // Send the JSON string to a dedicated service for safe parsing.
  data_decoder_.ParseJson(
      *hpke_public_key,
      base::BindOnce(&FeedbackService::VariationsExtractHpkePublicKey, this,
                     feedback_data, barrier_closure));
}

// Sample JSON string:
// {
//   "primaryKeyId": 123,
//   "key": [
//     {
//       "keyData": {
//         "typeUrl": "type.googleapis.com/google.crypto.tink.HpkePublicKey",
//         "value": "Base64Encoded HPKE Proto",
//         "keyMaterialType": "ASYMMETRIC_PUBLIC"
//       },
//       "status": "ENABLED",
//       "keyId": 123,
//       "outputPrefixType": "RAW"
//     }
//   ]
// }
void FeedbackService::VariationsExtractHpkePublicKey(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    base::RepeatingClosure barrier_closure,
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value() || !result->is_dict()) {
    LOG(ERROR) << "Failed to parse JSON or it's not a dictionary.";
    return VariationsFinished(false, barrier_closure);
  }

  const base::Value::Dict& json_dict = result->GetDict();
  const base::Value::List* key_list = json_dict.FindList("key");

  if (!key_list || key_list->empty()) {
    LOG(ERROR) << "Key list not found or empty.";
    return VariationsFinished(false, barrier_closure);
  }

  // Get the first item in the "key" list
  const base::Value& key_item = (*key_list)[0];
  const base::Value::Dict* key_dict = key_item.GetIfDict();

  if (!key_dict) {
    LOG(ERROR) << "Unexpected format in 'key' item.";
    return VariationsFinished(false, barrier_closure);
  }

  // Extract "keyData" dictionary
  const base::Value::Dict* key_data_dict = key_dict->FindDict("keyData");
  if (!key_data_dict) {
    LOG(ERROR) << "Failed to find 'keyData' dictionary.";
    return VariationsFinished(false, barrier_closure);
  }

  // Extract "value" from "keyData"
  const std::string* base64_serialized_proto_hpke =
      key_data_dict->FindString("value");
  if (!base64_serialized_proto_hpke) {
    LOG(ERROR) << "Failed to extract 'value' from 'keyData'.";
    return VariationsFinished(false, barrier_closure);
  }

  // std::string base64_proto_key = *base64_proto_keyp;
  std::string serialized_proto_hpke;

  if (!base::Base64Decode(*base64_serialized_proto_hpke,
                          &serialized_proto_hpke)) {
    LOG(ERROR) << "base64 decode of hpke proto failed";
    return VariationsFinished(false, barrier_closure);
  }
  userfeedback::HpkePublicKey key_proto;
  if (!key_proto.ParseFromString(serialized_proto_hpke)) {
    LOG(ERROR) << "Failed to parse HpkePublicKey.";
    return VariationsFinished(false, barrier_closure);
  }

  std::string hpke_public_key_string = key_proto.public_key();
  std::vector<uint8_t> hpke_public_key;
  hpke_public_key.assign(hpke_public_key_string.begin(),
                         hpke_public_key_string.end());
  VLOG(1) << "HPKE public KEY:" << base::HexEncode(hpke_public_key);
  return VariationsEncryptWithHpkeKey(hpke_public_key, feedback_data,
                                      barrier_closure);
  ;
}

void FeedbackService::VariationsEncryptWithHpkeKey(
    const std::vector<uint8_t>& hpke_public_key,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    base::RepeatingClosure barrier_closure) {
  std::string variations_string =
      variations::VariationsCommandLine::GetForCurrentProcess().ToString();
  if (variations_string.empty()) {
    LOG(ERROR) << "Unable to get valid variations.";
    return VariationsFinished(false, barrier_closure);
  }
  std::vector<uint8_t> variations(variations_string.begin(),
                                  variations_string.end());
  bssl::ScopedEVP_HPKE_CTX sender_context;

  // This vector will hold the encapsulated shared secret "enc" followed by the
  // symmetrically encrypted ciphertext "ct". Start with a size big enough for
  // the shared secret.
  std::vector<uint8_t> encrypted_variations(EVP_HPKE_MAX_ENC_LENGTH);
  size_t encapsulated_shared_secret_len;

  if (!EVP_HPKE_CTX_setup_sender(
          /*ctx=*/sender_context.get(),
          /*out_enc=*/encrypted_variations.data(),
          /*out_enc_len=*/&encapsulated_shared_secret_len,
          /*max_enc=*/encrypted_variations.size(),
          /*kem=*/EVP_hpke_x25519_hkdf_sha256(),
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/EVP_hpke_aes_256_gcm(),
          /*peer_public_key=*/hpke_public_key.data(),
          /*peer_public_key_len=*/hpke_public_key.size(),
          /*info=*/nullptr,
          /*info_len=*/0)) {
    LOG(ERROR) << "hpke setup failed";
    return VariationsFinished(false, barrier_closure);
  }
  encrypted_variations.resize(encapsulated_shared_secret_len +
                              variations.size() +
                              EVP_HPKE_CTX_max_overhead(sender_context.get()));
  base::span<uint8_t> ciphertext =
      base::span(encrypted_variations).subspan(encapsulated_shared_secret_len);
  size_t ciphertext_len;

  if (!EVP_HPKE_CTX_seal(
          /*ctx=*/sender_context.get(),
          /*out=*/ciphertext.data(),
          /*out_len=*/&ciphertext_len,
          /*max_out_len=*/ciphertext.size(),
          /*in=*/variations.data(),
          /*in_len*/ variations.size(),
          /*ad=*/nullptr,
          /*ad_len=*/0)) {
    LOG(ERROR) << "hpke seal failed";
    return VariationsFinished(false, barrier_closure);
  }
  encrypted_variations.resize(encapsulated_shared_secret_len + ciphertext_len);
  feedback_data->AddFile(
      kVariationsAttachmentName,
      std::string(encrypted_variations.begin(), encrypted_variations.end()));
  return VariationsFinished(true, barrier_closure);
}

void FeedbackService::VariationsFinished(
    bool variations_attached,
    base::RepeatingClosure barrier_closure) {
  if (variations_attached) {
    VLOG(1) << "variations attached to feedback report";
  } else {
    VLOG(1) << "variations not attached to feedback report";
  }
  std::move(barrier_closure).Run();
}

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions
