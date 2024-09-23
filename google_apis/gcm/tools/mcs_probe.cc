// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A standalone tool for testing MCS connections and the MCS client on their
// own.

#include <stdint.h>

#include <cstddef>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "google_apis/gcm/base/fake_encryptor.h"
#include "google_apis/gcm/base/mcs_message.h"
#include "google_apis/gcm/base/mcs_util.h"
#include "google_apis/gcm/engine/checkin_request.h"
#include "google_apis/gcm/engine/connection_factory_impl.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/engine/mcs_client.h"
#include "google_apis/gcm/monitoring/fake_gcm_stats_recorder.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_verifier.h"
#include "net/dns/host_resolver.h"
#include "net/http/http_auth_handler_factory.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_auth_scheme.h"
#include "net/log/file_net_log_observer.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/proxy_resolving_socket.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "url/scheme_host_port.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#endif

// This is a simple utility that initializes an mcs client and
// prints out any events.
namespace gcm {
namespace {

const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  15000,  // 15 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.5,  // 50%.

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 60 * 5, // 5 minutes.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

// Default values used to communicate with the check-in server.
const char kChromeVersion[] = "Chrome MCS Probe";

// The default server to communicate with.
const char kMCSServerHost[] = "mtalk.google.com";
const uint16_t kMCSServerPort = 5228;

// Command line switches.
const char kRMQFileName[] = "rmq_file";
const char kAndroidIdSwitch[] = "android_id";
const char kSecretSwitch[] = "secret";
const char kLogFileSwitch[] = "log-file";
const char kIgnoreCertSwitch[] = "ignore-certs";
const char kServerHostSwitch[] = "host";
const char kServerPortSwitch[] = "port";

void MessageReceivedCallback(const MCSMessage& message) {
  LOG(INFO) << "Received message with id "
            << GetPersistentId(message.GetProtobuf()) << " and tag "
            << static_cast<int>(message.tag());

  if (message.tag() == kDataMessageStanzaTag) {
    const mcs_proto::DataMessageStanza& data_message =
        reinterpret_cast<const mcs_proto::DataMessageStanza&>(
            message.GetProtobuf());
    DVLOG(1) << "  to: " << data_message.to();
    DVLOG(1) << "  from: " << data_message.from();
    DVLOG(1) << "  category: " << data_message.category();
    DVLOG(1) << "  sent: " << data_message.sent();
    for (int i = 0; i < data_message.app_data_size(); ++i) {
      DVLOG(1) << "  App data " << i << " "
               << data_message.app_data(i).key() << " : "
               << data_message.app_data(i).value();
    }
  }
}

void MessageSentCallback(int64_t user_serial_number,
                         const std::string& app_id,
                         const std::string& message_id,
                         MCSClient::MessageSendStatus status) {
  LOG(INFO) << "Message sent. Serial number: " << user_serial_number
            << " Application ID: " << app_id
            << " Message ID: " << message_id
            << " Message send status: " << status;
}

// A cert verifier that access all certificates.
class MyTestCertVerifier : public net::CertVerifier {
 public:
  MyTestCertVerifier() {}
  ~MyTestCertVerifier() override {}

  int Verify(const RequestParams& params,
             net::CertVerifyResult* verify_result,
             net::CompletionOnceCallback callback,
             std::unique_ptr<Request>* out_req,
             const net::NetLogWithSource& net_log) override {
    verify_result->Reset();
    verify_result->verified_cert = params.certificate();
    return net::OK;
  }
  void SetConfig(const Config& config) override {}
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
};

class MCSProbeAuthPreferences : public net::HttpAuthPreferences {
 public:
  MCSProbeAuthPreferences() {}
  ~MCSProbeAuthPreferences() override {}

  bool NegotiateDisableCnameLookup() const override { return false; }
  bool NegotiateEnablePort() const override { return false; }
  bool CanUseDefaultCredentials(
      const url::SchemeHostPort& auth_scheme_host_port) const override {
    return false;
  }
  net::HttpAuth::DelegationType GetDelegationType(
      const url::SchemeHostPort& auth_scheme_host_port) const override {
    return net::HttpAuth::DelegationType::kNone;
  }
};

class MCSProbe {
 public:
  explicit MCSProbe(const base::CommandLine& command_line);
  ~MCSProbe();

  void Start();

  uint64_t android_id() const { return android_id_; }
  uint64_t secret() const { return secret_; }

 private:
  void RequestProxyResolvingSocketFactory(
      mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
          receiver);
  void CheckIn();
  void InitializeNetworkState();

  void LoadCallback(std::unique_ptr<GCMStore::LoadResult> load_result);
  void UpdateCallback(bool success);
  void ErrorCallback();
  void OnCheckInCompleted(
      net::HttpStatusCode response_code,
      const checkin_proto::AndroidCheckinResponse& checkin_response);
  void StartMCSLogin();

  base::DefaultClock clock_;

  base::CommandLine command_line_;

  base::FilePath gcm_store_path_;
  uint64_t android_id_;
  uint64_t secret_;
  std::string server_host_;
  int server_port_;

  // Network state.
  std::unique_ptr<network::TestNetworkConnectionTracker>
      network_connection_tracker_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  net::NetLog* net_log_;
  std::unique_ptr<net::FileNetLogObserver> logger_;
  MCSProbeAuthPreferences http_auth_preferences_;

  FakeGCMStatsRecorder recorder_;
  std::unique_ptr<GCMStore> gcm_store_;
  std::unique_ptr<MCSClient> mcs_client_;
  std::unique_ptr<CheckinRequest> checkin_request_;

  std::unique_ptr<ConnectionFactoryImpl> connection_factory_;

  base::Thread file_thread_;

  std::unique_ptr<network::NetworkContext> network_context_;
  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<base::RunLoop> run_loop_;
};

MCSProbe::MCSProbe(const base::CommandLine& command_line)
    : command_line_(command_line),
      gcm_store_path_(base::FilePath(FILE_PATH_LITERAL("gcm_store"))),
      android_id_(0),
      secret_(0),
      server_port_(0),
      network_connection_tracker_(
          network::TestNetworkConnectionTracker::CreateInstance()),
      net_log_(net::NetLog::Get()),
      file_thread_("FileThread") {
  network_connection_tracker_->SetConnectionType(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  if (command_line.HasSwitch(kRMQFileName)) {
    gcm_store_path_ = command_line.GetSwitchValuePath(kRMQFileName);
  }
  if (command_line.HasSwitch(kAndroidIdSwitch)) {
    base::StringToUint64(command_line.GetSwitchValueASCII(kAndroidIdSwitch),
                         &android_id_);
  }
  if (command_line.HasSwitch(kSecretSwitch)) {
    base::StringToUint64(command_line.GetSwitchValueASCII(kSecretSwitch),
                         &secret_);
  }
  server_host_ = kMCSServerHost;
  if (command_line.HasSwitch(kServerHostSwitch)) {
    server_host_ = command_line.GetSwitchValueASCII(kServerHostSwitch);
  }
  server_port_ = kMCSServerPort;
  if (command_line.HasSwitch(kServerPortSwitch)) {
    base::StringToInt(command_line.GetSwitchValueASCII(kServerPortSwitch),
                      &server_port_);
  }
}

MCSProbe::~MCSProbe() {
  if (logger_)
    logger_->StopObserving(nullptr, base::OnceClosure());
  file_thread_.Stop();
}

void MCSProbe::Start() {
  file_thread_.Start();
  InitializeNetworkState();
  std::vector<GURL> endpoints(
      1, GURL("https://" +
              net::HostPortPair(server_host_, server_port_).ToString()));

  connection_factory_ = std::make_unique<ConnectionFactoryImpl>(
      endpoints, kDefaultBackoffPolicy,
      base::BindRepeating(&MCSProbe::RequestProxyResolvingSocketFactory,
                          base::Unretained(this)),
      base::SingleThreadTaskRunner::GetCurrentDefault(), &recorder_,
      network_connection_tracker_.get());
  gcm_store_ = std::make_unique<GCMStoreImpl>(
      gcm_store_path_, file_thread_.task_runner(),
      std::make_unique<FakeEncryptor>());

  mcs_client_ = std::make_unique<MCSClient>(
      "probe", &clock_, connection_factory_.get(), gcm_store_.get(),
      base::SingleThreadTaskRunner::GetCurrentDefault(), &recorder_);
  run_loop_ = std::make_unique<base::RunLoop>();
  gcm_store_->Load(
      GCMStore::CREATE_IF_MISSING,
      base::BindOnce(&MCSProbe::LoadCallback, base::Unretained(this)));
  run_loop_->Run();
}

void MCSProbe::LoadCallback(std::unique_ptr<GCMStore::LoadResult> load_result) {
  DCHECK(load_result->success);
  if (android_id_ != 0 && secret_ != 0) {
    DVLOG(1) << "Presetting MCS id " << android_id_;
    load_result->device_android_id = android_id_;
    load_result->device_security_token = secret_;
    gcm_store_->SetDeviceCredentials(
        android_id_, secret_,
        base::BindOnce(&MCSProbe::UpdateCallback, base::Unretained(this)));
  } else {
    android_id_ = load_result->device_android_id;
    secret_ = load_result->device_security_token;
    DVLOG(1) << "Loaded MCS id " << android_id_;
  }
  mcs_client_->Initialize(
      base::BindRepeating(&MCSProbe::ErrorCallback, base::Unretained(this)),
      base::BindRepeating(&MessageReceivedCallback),
      base::BindRepeating(&MessageSentCallback), std::move(load_result));

  if (!android_id_ || !secret_) {
    DVLOG(1) << "Checkin to generate new MCS credentials.";
    CheckIn();
    return;
  }

  StartMCSLogin();
}

void MCSProbe::UpdateCallback(bool success) {
}

void MCSProbe::InitializeNetworkState() {
  if (command_line_.HasSwitch(kLogFileSwitch)) {
    base::FilePath log_path = command_line_.GetSwitchValuePath(kLogFileSwitch);
    net::NetLogCaptureMode capture_mode =
        net::NetLogCaptureMode::kIncludeSensitive;
    logger_ = net::FileNetLogObserver::CreateUnbounded(log_path, capture_mode,
                                                       nullptr);
    logger_->StartObserving(net_log_);
  }

  net::URLRequestContextBuilder builder;
  builder.set_net_log(net_log_);
  builder.set_host_resolver(
      net::HostResolver::CreateStandaloneResolver(net_log_));
  http_auth_preferences_.set_allowed_schemes(
      std::set<std::string>{net::kBasicAuthScheme});
  builder.SetHttpAuthHandlerFactory(
      net::HttpAuthHandlerRegistryFactory::Create(&http_auth_preferences_));
  builder.set_proxy_resolution_service(
      net::ConfiguredProxyResolutionService::CreateDirect());

  if (command_line_.HasSwitch(kIgnoreCertSwitch))
    builder.SetCertVerifier(std::make_unique<MyTestCertVerifier>());

  url_request_context_ = builder.Build();

  // Wrap it up with network service APIs.
  network_context_ = std::make_unique<network::NetworkContext>(
      nullptr /* network_service */,
      network_context_remote_.BindNewPipeAndPassReceiver(),
      url_request_context_.get(),
      /*cors_exempt_header_list=*/std::vector<std::string>());
  auto url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  url_loader_factory_params->is_orb_enabled = false;
  network_context_->CreateURLLoaderFactory(
      url_loader_factory_.BindNewPipeAndPassReceiver(),
      std::move(url_loader_factory_params));
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          url_loader_factory_.get());
}

void MCSProbe::ErrorCallback() {
  LOG(INFO) << "MCS error happened";
}

void MCSProbe::RequestProxyResolvingSocketFactory(
    mojo::PendingReceiver<network::mojom::ProxyResolvingSocketFactory>
        receiver) {
  return network_context_->CreateProxyResolvingSocketFactory(
      std::move(receiver));
}

void MCSProbe::CheckIn() {
  LOG(INFO) << "Check-in request initiated.";
  checkin_proto::ChromeBuildProto chrome_build_proto;
  chrome_build_proto.set_platform(
      checkin_proto::ChromeBuildProto::PLATFORM_LINUX);
  chrome_build_proto.set_channel(
      checkin_proto::ChromeBuildProto::CHANNEL_CANARY);
  chrome_build_proto.set_chrome_version(kChromeVersion);

  CheckinRequest::RequestInfo request_info(0, 0,
                                           std::map<std::string, std::string>(),
                                           std::string(), chrome_build_proto);

  checkin_request_ = std::make_unique<CheckinRequest>(
      GServicesSettings().GetCheckinURL(), request_info, kDefaultBackoffPolicy,
      base::BindOnce(&MCSProbe::OnCheckInCompleted, base::Unretained(this)),
      shared_url_loader_factory_,
      base::SingleThreadTaskRunner::GetCurrentDefault(), &recorder_);
  checkin_request_->Start();
}

void MCSProbe::OnCheckInCompleted(
    net::HttpStatusCode response_code,
    const checkin_proto::AndroidCheckinResponse& checkin_response) {
  bool success = response_code == net::HTTP_OK &&
                 checkin_response.has_android_id() &&
                 checkin_response.android_id() != 0UL &&
                 checkin_response.has_security_token() &&
                 checkin_response.security_token() != 0UL;
  LOG(INFO) << "Check-in request completion "
            << (success ? "success!" : "failure!");

  if (!success)
    return;

  android_id_ = checkin_response.android_id();
  secret_ = checkin_response.security_token();

  gcm_store_->SetDeviceCredentials(
      android_id_, secret_,
      base::BindOnce(&MCSProbe::UpdateCallback, base::Unretained(this)));

  StartMCSLogin();
}

void MCSProbe::StartMCSLogin() {
  LOG(INFO) << "MCS login initiated.";

  mcs_client_->Login(android_id_, secret_);
}

int MCSProbeMain(int argc, char* argv[]) {
  base::AtExitManager exit_manager;

  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  mojo::core::Init();

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("MCSProbe");

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  MCSProbe mcs_probe(command_line);
  mcs_probe.Start();

  base::RunLoop run_loop;
  run_loop.Run();

  base::ThreadPoolInstance::Get()->Shutdown();

  return 0;
}

}  // namespace
}  // namespace gcm

int main(int argc, char* argv[]) {
  return gcm::MCSProbeMain(argc, argv);
}
