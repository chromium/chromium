#include "extensions/browser/api/read_server_uds/read_server_uds_api.h"

#include <cstddef>
#include <limits>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/span.h"  // For base::span if needed
#include "base/debug/debugging_buildflags.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"  // For base::FilePath utilities
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"  // Include for content::BrowserThread
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/read_server_uds/ml_server_uds.h"
#include "extensions/browser/event_router.h"
#include "net/base/io_buffer.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/zlib/google/zip_reader.h"
#include "third_party/zlib/google/zip_writer.h"
#include "url/gurl.h"

/// tmp/shared-sockets/echo_socket
namespace extensions {

constexpr char kMLServerUDSPath[] = "/tmp/shared-sockets/echo_socket";

// ALL lable for ML server function handler
constexpr char kReadServerUdsReadDataFunctionLable[] = "LABEL_READ_DATA";
constexpr char kReadServerUdsSendDataFunctionLable[] = "LABEL_SEND_DATA";
constexpr char kReadServerUdsLoadModelBERTFunctionLable[] =
    "LABEL_LOAD_MODEL_BERT";
constexpr char kReadServerUdsInferSingleBERTFunctionLable[] =
    "LABEL_INFER_MODEL_BERT";

// -------------------------
// Read Server Read Data UDS
// -------------------------
ReadServerUdsReadDataFunction::ReadServerUdsReadDataFunction() = default;

ReadServerUdsReadDataFunction::~ReadServerUdsReadDataFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "Function was destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerUdsReadDataFunction::Run() {
  AddRef();  // async

  auto ml_server = std::make_unique<extensions::MLServerUDS>(
      kMLServerUDSPath, kReadServerUdsReadDataFunctionLable);

  std::string payload = "GET /data\n";

  ml_server->Send(payload,
                  base::BindOnce(&ReadServerUdsReadDataFunction::OnSuccess,
                                 weak_ptr_factory_.GetWeakPtr()),
                  base::BindOnce(&ReadServerUdsReadDataFunction::OnError,
                                 weak_ptr_factory_.GetWeakPtr()));

  // Important: hold the instance if needed
  ml_server_ = std::move(ml_server);

  return RespondLater();
}

void ReadServerUdsReadDataFunction::OnSuccess(std::string result) {
  Respond(WithArguments(base::Value(result)));
  Release();
}

void ReadServerUdsReadDataFunction::OnError(std::string error_msg) {
  Respond(Error(error_msg));
  Release();
}

void ReadServerUdsReadDataFunction::OnResponded() {
  LOG(INFO) << "ReadServerUdsReadDataFunction::OnResponded() Cleaning up";

  if (ml_server_) {
    ml_server_->Clear();  // First clean up state
    ml_server_.reset();   // Then destroy safely
  }

  // Other cleanup if needed
}

// -------------------------
// Read Server Send Data UDS
// -------------------------
// Constructor for ReadServerUdsSendDataFunction
ReadServerUdsSendDataFunction::ReadServerUdsSendDataFunction() = default;

// Destructor for ReadServerUdsSendDataFunction
ReadServerUdsSendDataFunction::~ReadServerUdsSendDataFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "Function was destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerUdsSendDataFunction::Run() {
  LOG(INFO) << "ReadServerUdsSendDataFunction::Run() called";

  // Validate the presence of arguments
  EXTENSION_FUNCTION_VALIDATE(has_args());

  // Validate that arguments exist and the first argument is a string
  const base::Value::List& args_list = args();
  EXTENSION_FUNCTION_VALIDATE(args_list.size() > 0);
  const base::Value& arg = args_list[0];
  EXTENSION_FUNCTION_VALIDATE(arg.is_string());

  std::string payload = arg.GetString();

  AddRef();  // async

  auto ml_server = std::make_unique<extensions::MLServerUDS>(
      kMLServerUDSPath, kReadServerUdsSendDataFunctionLable);

  ml_server->Send(payload,
                  base::BindOnce(&ReadServerUdsSendDataFunction::OnSuccess,
                                 weak_ptr_factory_.GetWeakPtr()),
                  base::BindOnce(&ReadServerUdsSendDataFunction::OnError,
                                 weak_ptr_factory_.GetWeakPtr()));

  // Important: hold the instance if needed
  ml_server_ = std::move(ml_server);

  return RespondLater();
}

void ReadServerUdsSendDataFunction::OnSuccess(std::string result) {
  Respond(WithArguments(base::Value(result)));
  Release();
}

void ReadServerUdsSendDataFunction::OnError(std::string error_msg) {
  Respond(Error(error_msg));
  Release();
}

void ReadServerUdsSendDataFunction::OnResponded() {
  LOG(INFO) << "ReadServerUdsSendDataFunction::OnResponded() Cleaning up";

  if (ml_server_) {
    ml_server_->Clear();  // First clean up state
    ml_server_.reset();   // Then destroy safely
  }

  // Other cleanup if needed
}

//------------------------
// Upload Training Data
//------------------------
ReadServerUdsUploadTrainingDataFunction::
    ReadServerUdsUploadTrainingDataFunction()
    : chunk_size_(1024 * 1024),  // 1 MB
      offset_(0) {}              // Removed weak_ptr_factory_ initializer

ReadServerUdsUploadTrainingDataFunction::
    ~ReadServerUdsUploadTrainingDataFunction() = default;

ExtensionFunction::ResponseAction
ReadServerUdsUploadTrainingDataFunction::Run() {
  // Increment reference count to keep the function alive.
  AddRef();

  // Start the data generation process on a background thread.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &ReadServerUdsUploadTrainingDataFunction::GenerateSyntheticData,
          base::Unretained(this)));

  return RespondLater();
}

void ReadServerUdsUploadTrainingDataFunction::GenerateSyntheticData() {
  // Generate synthetic training data.
  const size_t num_records = 25000;
  const size_t num_features = 50;
  const size_t num_clusters = 7;  // Number of clusters/classes

  std::ostringstream oss;

  // Initialize random seed
  std::srand(static_cast<unsigned int>(std::time(0)));

  // Generate random cluster centers for each label
  std::vector<double> cluster_centers;
  for (size_t i = 0; i < num_clusters; ++i) {
    cluster_centers.push_back(0.2 + static_cast<double>(std::rand()) /
                                        RAND_MAX * 0.6);
  }

  for (size_t i = 0; i < num_records; ++i) {
    // Assign a label by selecting a random cluster
    size_t label = i % num_clusters;  // Cycling through clusters
    double cluster_center = cluster_centers[label];

    for (size_t j = 0; j < num_features; ++j) {
      // Generate a value around the cluster center with random variation in
      // range [-0.2, 0.2]
      double random_variation =
          static_cast<double>(std::rand()) / RAND_MAX * 0.4 - 0.2;
      double value = cluster_center + random_variation;

      // Clamp value to [0.0, 1.0]
      value = std::max(0.0, std::min(1.0, value));
      oss << value << ",";
    }
    // Append the label as the last column
    oss << label << "\n";
  }

  training_data_ = oss.str();

  // Post back to the UI
  // threoverleaf.com/latex/templates/iit-kgp-mtp-thesis-template/hgprtqycxzmbad
  // to start uploading data.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReadServerUdsUploadTrainingDataFunction::StartUploadingData,
          base::Unretained(this)));
}

void ReadServerUdsUploadTrainingDataFunction::StartUploadingData() {
  offset_ = 0;
  UploadNextChunk();
}

void ReadServerUdsUploadTrainingDataFunction::UploadNextChunk() {
  if (offset_ >= training_data_.size()) {
    // All chunks uploaded.
    RespondWithSuccess();
    return;
  }

  size_t remaining_size = training_data_.size() - offset_;
  size_t current_chunk_size = std::min(chunk_size_, remaining_size);
  std::string chunk = training_data_.substr(offset_, current_chunk_size);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url =
      GURL("http://localhost:5000/upload_training_data_chunk");
  resource_request->method = "POST";

  // Add headers indicating the offset and total size
  resource_request->headers.SetHeader("X-Chunk-Offset",
                                      base::NumberToString(offset_));
  resource_request->headers.SetHeader(
      "X-Total-Size", base::NumberToString(training_data_.size()));

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("upload_training_data_chunk", R"(
        semantics {
          sender: "Read Server Extension"
          description: "Uploads a chunk of synthetic training data to the server."
          trigger: "User action in the extension."
          data: "Chunked synthetic training data."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This extension is not part of Chrome and thus cannot be disabled or configured."
        }
      )");

  // Ensure url_loader_ is declared
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(chunk, "application/octet-stream");

  content::StoragePartition* storage_partition =
      browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerUdsUploadTrainingDataFunction::OnChunkUploaded,
                     base::Unretained(this)));
}

void ReadServerUdsUploadTrainingDataFunction::OnChunkUploaded(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    RespondWithError("Failed to upload chunk");
    return;
  }

  // Log response if needed.
  LOG(INFO) << "Chunk uploaded: " << *response_body;

  // Update offset.
  offset_ += chunk_size_;

  // Upload next chunk.
  UploadNextChunk();
}

void ReadServerUdsUploadTrainingDataFunction::RespondWithError(
    const std::string& error_message) {
  Respond(Error(error_message));

  // Decrement reference count.
  Release();
}

void ReadServerUdsUploadTrainingDataFunction::RespondWithSuccess() {
  Respond(WithArguments("Synthetic training data uploaded successfully"));

  // Decrement reference count.
  Release();
}

// Training on MNIST
ReadServerUdsTrainModelFunction::ReadServerUdsTrainModelFunction() = default;

ReadServerUdsTrainModelFunction::~ReadServerUdsTrainModelFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "TrainModel function destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerUdsTrainModelFunction::Run() {
  LOG(INFO) << "ReadServerUdsTrainModelFunction::Run() called";
  AddRef();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/train_model");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  // In this simple example, no payload is required, so we send an empty JSON.
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      net::DefineNetworkTrafficAnnotation("train_model", R"(
        semantics {
          sender: "Read Server API"
          description: "Requests training of a simple model on the server."
          trigger: "User action in the extension."
          data: "No user data is sent."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be disabled by settings."
        }
  )"));
  url_loader_->AttachStringForUpload("{}", "application/json");

  content::StoragePartition* storage_partition =
      browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerUdsTrainModelFunction::OnTrainModelResponse,
                     base::Unretained(this)));

  return RespondLater();
}

void ReadServerUdsTrainModelFunction::OnTrainModelResponse(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Training request failed."));
  } else {
    Respond(WithArguments(*response_body));
  }
  Release();
}

// Constructor
ReadServerUdsInferenceFunction::ReadServerUdsInferenceFunction() = default;

// Destructor
ReadServerUdsInferenceFunction::~ReadServerUdsInferenceFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "Function was destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

// Run method to send POST request to /infer endpoint
ExtensionFunction::ResponseAction ReadServerUdsInferenceFunction::Run() {
  LOG(INFO) << "ReadServerUdsInferenceFunction::Run() called";

  // Validate that arguments exist and the first argument is a string
  EXTENSION_FUNCTION_VALIDATE(args().size() > 0);
  const base::Value& arg = args()[0];
  EXTENSION_FUNCTION_VALIDATE(arg.is_string());

  const std::string& features = arg.GetString();

  // Increment reference count to keep the function alive until response is
  // received
  AddRef();

  // Define the request to the /infer endpoint
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/infer");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  // Convert features string to JSON format for the request body
  std::string json_data = "{\"features\": \"" + features + "\"}";

  // Initialize SimpleURLLoader for sending the request
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      net::DefineNetworkTrafficAnnotation("inference_request", R"(
    semantics {
      sender: "Read Server API"
      description: "Requests server for inference based on 50 input features."
      trigger: "User action in the extension."
      data: "User-provided features."
      destination: LOCAL
    }
    policy {
      cookies_allowed: NO
      setting: "This request cannot be disabled by settings."
    }
  )"));
  url_loader_->AttachStringForUpload(json_data, "application/json");

  content::StoragePartition* storage_partition =
      browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerUdsInferenceFunction::OnInferenceResponse,
                     base::Unretained(this)));

  return RespondLater();
}

// Callback to handle the response from the /infer endpoint
void ReadServerUdsInferenceFunction::OnInferenceResponse(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Failed to get inference result from the server"));
    Release();  // Decrement reference count
    return;
  }

  auto json = base::JSONReader::Read(*response_body);
  if (!json.has_value()) {
    Respond(Error("Failed to parse JSON response from the server"));
    Release();  // Decrement reference count
    return;
  }

  Respond(WithArguments(response_body->c_str()));
  Release();  // Decrement reference count after responding
}

// -------------------------
// Load Model BERT Endpoint
// -------------------------
ReadServerUdsLoadModelBERTFunction::ReadServerUdsLoadModelBERTFunction() =
    default;
ReadServerUdsLoadModelBERTFunction::~ReadServerUdsLoadModelBERTFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "LoadModelBERT function destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerUdsLoadModelBERTFunction::Run() {
  LOG(INFO) << "ReadServerUdsLoadModelBERTFunction::Run() called";
  AddRef();  // async

  auto ml_server = std::make_unique<extensions::MLServerUDS>(
      kMLServerUDSPath, kReadServerUdsLoadModelBERTFunctionLable);

  std::string payload = "init the bert model\n";

  ml_server->Send(payload,
                  base::BindOnce(&ReadServerUdsLoadModelBERTFunction::OnSuccess,
                                 weak_ptr_factory_.GetWeakPtr()),
                  base::BindOnce(&ReadServerUdsLoadModelBERTFunction::OnError,
                                 weak_ptr_factory_.GetWeakPtr()));

  // Important: hold the instance if needed
  ml_server_ = std::move(ml_server);

  return RespondLater();
}

void ReadServerUdsLoadModelBERTFunction::OnSuccess(std::string result) {
  Respond(WithArguments(base::Value(result)));
  Release();
}

void ReadServerUdsLoadModelBERTFunction::OnError(std::string error_msg) {
  Respond(Error(error_msg));
  Release();
}

void ReadServerUdsLoadModelBERTFunction::OnResponded() {
  LOG(INFO) << "ReadServerUdsLoadModelBERTFunction::OnResponded() Cleaning up";

  if (ml_server_) {
    ml_server_->Clear();  // First clean up state
    ml_server_.reset();   // Then destroy safely
  }

  // Other cleanup if needed
}

// -------------------------
// Single Inference BERT Endpoint
// -------------------------
ReadServerUdsInferSingleBERTFunction::ReadServerUdsInferSingleBERTFunction() =
    default;
ReadServerUdsInferSingleBERTFunction::~ReadServerUdsInferSingleBERTFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "InferSingleBERT function destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerUdsInferSingleBERTFunction::Run() {
  LOG(INFO) << "ReadServerUdsInferSingleBERTFunction::Run() called";

  AddRef();  // async

  // Validate the presence of arguments
  EXTENSION_FUNCTION_VALIDATE(has_args());

  // Validate that arguments exist and the first argument is a string
  // The IDL defines the parameter as a DOMString, so extract it as a string.
  const base::Value::List& args_list = args();
  EXTENSION_FUNCTION_VALIDATE(args_list.size() > 0);
  const base::Value& arg = args_list[0];
  EXTENSION_FUNCTION_VALIDATE(arg.is_string());

  std::string payload = arg.GetString();

  LOG(INFO) << "Payload " << payload;

  auto ml_server = std::make_unique<extensions::MLServerUDS>(
      kMLServerUDSPath, kReadServerUdsInferSingleBERTFunctionLable);

  ml_server->Send(
      payload,
      base::BindOnce(&ReadServerUdsInferSingleBERTFunction::OnSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ReadServerUdsInferSingleBERTFunction::OnError,
                     weak_ptr_factory_.GetWeakPtr()));

  // Important: hold the instance if needed
  ml_server_ = std::move(ml_server);

  return RespondLater();
}

void ReadServerUdsInferSingleBERTFunction::OnSuccess(std::string result) {
  Respond(WithArguments(base::Value(result)));
  Release();
}

void ReadServerUdsInferSingleBERTFunction::OnError(std::string error_msg) {
  Respond(Error(error_msg));
  Release();
}

void ReadServerUdsInferSingleBERTFunction::OnResponded() {
  LOG(INFO)
      << "ReadServerUdsInferSingleBERTFunction::OnResponded() Cleaning up";

  if (ml_server_) {
    ml_server_->Clear();  // First clean up state
    ml_server_.reset();   // Then destroy safely
  }

  // Other cleanup if needed
}

// -------------------------
// Batch Inference BERT Endpoint
// -------------------------
ReadServerUdsInferBatchBERTFunction::ReadServerUdsInferBatchBERTFunction() =
    default;
ReadServerUdsInferBatchBERTFunction::~ReadServerUdsInferBatchBERTFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "InferBatchBERT function destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerUdsInferBatchBERTFunction::Run() {
  LOG(INFO) << "ReadServerUdsInferBatchBERTFunction::Run() called";

  // Validate that input is a list.
  EXTENSION_FUNCTION_VALIDATE(args().size() > 0);
  const base::Value& arg = args()[0];
  if (!arg.is_list()) {
    return RespondNow(Error("Expected a list of question-context objects."));
  }

  // Serialize the list to a JSON string.
  std::string json_data;
  if (!base::JSONWriter::Write(arg, &json_data)) {
    return RespondNow(Error("Failed to serialize input JSON."));
  }

  AddRef();
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/infer_batch_bert");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("infer_batch_bert", R"(
        semantics {
          sender: "Read Server API"
          description: "Requests batch inference from MobileBERT."
          trigger: "User action in the extension."
          data: "A JSON array where each element is an object with 'question' and 'context'."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be disabled by settings."
        }
      )");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(json_data, "application/json");

  content::StoragePartition* storage_partition =
      browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerUdsInferBatchBERTFunction::OnResponse,
                     base::Unretained(this)));
  return RespondLater();
}

void ReadServerUdsInferBatchBERTFunction::OnResponse(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Batch inference request failed."));
  } else {
    Respond(WithArguments(*response_body));
  }
  Release();
}

}  // namespace extensions
