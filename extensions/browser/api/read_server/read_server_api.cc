#include "extensions/browser/api/read_server/read_server_api.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "extensions/browser/event_router.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/zlib/google/zip_writer.h"
#include "third_party/zlib/google/zip_reader.h"
#include "base/containers/span.h"  // For base::span if needed
#include "base/containers/fixed_flat_set.h"  
#include "base/strings/string_util.h"  // For base::FilePath utilities
#include <vector>
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"  // Include for content::BrowserThread
#include "base/rand_util.h"   
#include <limits>             

namespace extensions {

// Constructor for ReadServerReadDataFunction
ReadServerReadDataFunction::ReadServerReadDataFunction() = default;

ReadServerReadDataFunction::~ReadServerReadDataFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "Function was destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerReadDataFunction::Run() {
  LOG(INFO) << "ReadServerReadDataFunction::Run() called";

  if (!render_frame_host()) {
    return RespondNow(Error("Invalid frame"));
  }

  AddRef();
  
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/data");
  resource_request->method = "GET";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("read_server_read_data", R"(
        semantics {
          sender: "Read Server API"
          description: "Fetches JSON data from a local server."
          trigger: "User action in the extension."
          data: "No user overleaf.com/latex/templates/iit-kgp-mtp-thesis-template/hgprtqycxzmbdata is sent."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be disabled by settings."
        }
      )");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request), traffic_annotation);

  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerReadDataFunction::OnJsonLoaded, weak_ptr_factory_.GetWeakPtr()));

  return RespondLater();
}

void ReadServerReadDataFunction::OnJsonLoaded(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Failed to load JSON from server"));
    Release();
    return;
  }

  auto json = base::JSONReader::Read(*response_body);
  if (!json.has_value()) {
    Respond(Error("Failed to parse JSON response"));
    Release();
    return;
  }

  Respond(WithArguments(response_body->c_str()));
  Release();
}

void ReadServerReadDataFunction::OnResponded() {
  url_loader_.reset();
}

// Constructor for ReadServerSendDataFunction
ReadServerSendDataFunction::ReadServerSendDataFunction() = default;

// Destructor for ReadServerSendDataFunction
ReadServerSendDataFunction::~ReadServerSendDataFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "Function was destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerSendDataFunction::Run() {
  LOG(INFO) << "ReadServerSendDataFunction::Run() called";

  // Validate the presence of arguments
  EXTENSION_FUNCTION_VALIDATE(has_args());

  // Validate that arguments exist and the first argument is a string
  const base::Value::List& args_list = args();
  EXTENSION_FUNCTION_VALIDATE(args_list.size() > 0);
  const base::Value& arg = args_list[0];
  EXTENSION_FUNCTION_VALIDATE(arg.is_string());

  // Extract the message from the arguments
  // const std::string& message = arg.GetString();
  const std::string& message = "This message is being sent to the server!";
  LOG(INFO) << "Sending message: " << message;

  AddRef();  // Keep the function alive until the request is completed

  // Create and send the POST request
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/data");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  std::string json_data = "{\"message\": \"" + message + "\"}";
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      net::DefineNetworkTrafficAnnotation("send_data_to_server", R"(
        semantics {
          sender: "Read Server API"
          description: "Sends data to the server."
          trigger: "User action in the extension."
          data: "Message data is sent."
          destination: LOCAL
        }
      )"));

  url_loader_->AttachStringForUpload(json_data, "application/json");

  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerSendDataFunction::OnDataSent,
                     weak_ptr_factory_.GetWeakPtr()));

  LOG(INFO) << "ReadServerSendDataFunction::Run() completed, request started";
  return RespondLater();
}

void ReadServerSendDataFunction::OnDataSent(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Failed to send data to the server"));
    Release();
    return;
  }

  Respond(WithArguments(response_body->c_str()));
  Release();
}

void ReadServerSendDataFunction::OnResponded() {
  url_loader_.reset();
}


ReadServerUploadTrainingDataFunction::ReadServerUploadTrainingDataFunction()
    : chunk_size_(1024 * 1024),  // 1 MB
      offset_(0) {}  // Removed weak_ptr_factory_ initializer

ReadServerUploadTrainingDataFunction::~ReadServerUploadTrainingDataFunction() = default;

ExtensionFunction::ResponseAction ReadServerUploadTrainingDataFunction::Run() {
  // Increment reference count to keep the function alive.
  AddRef();

  // Start the data generation process on a background thread.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock()},
      base::BindOnce(&ReadServerUploadTrainingDataFunction::GenerateSyntheticData,
                     base::Unretained(this)));

  return RespondLater();
}

void ReadServerUploadTrainingDataFunction::GenerateSyntheticData() {
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
      cluster_centers.push_back(0.2 + static_cast<double>(std::rand()) / RAND_MAX * 0.6);
  }

  for (size_t i = 0; i < num_records; ++i) {
      // Assign a label by selecting a random cluster
      size_t label = i % num_clusters;  // Cycling through clusters
      double cluster_center = cluster_centers[label];
      
      for (size_t j = 0; j < num_features; ++j) {
          // Generate a value around the cluster center with random variation in range [-0.2, 0.2]
          double random_variation = static_cast<double>(std::rand()) / RAND_MAX * 0.4 - 0.2;
          double value = cluster_center + random_variation;

          // Clamp value to [0.0, 1.0]
          value = std::max(0.0, std::min(1.0, value));
          oss << value << ",";
      }
      // Append the label as the last column
      oss << label << "\n";
  }

  training_data_ = oss.str();

  // Post back to the UI threoverleaf.com/latex/templates/iit-kgp-mtp-thesis-template/hgprtqycxzmbad to start uploading data.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadServerUploadTrainingDataFunction::StartUploadingData,
                     base::Unretained(this)));
}

void ReadServerUploadTrainingDataFunction::StartUploadingData() {
  offset_ = 0;
  UploadNextChunk();
}

void ReadServerUploadTrainingDataFunction::UploadNextChunk() {
  if (offset_ >= training_data_.size()) {
    // All chunks uploaded.
    RespondWithSuccess();
    return;
  }

  size_t remaining_size = training_data_.size() - offset_;
  size_t current_chunk_size = std::min(chunk_size_, remaining_size);
  std::string chunk = training_data_.substr(offset_, current_chunk_size);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/upload_training_data_chunk");
  resource_request->method = "POST";

  // Add headers indicating the offset and total size
  resource_request->headers.SetHeader("X-Chunk-Offset", base::NumberToString(offset_));
  resource_request->headers.SetHeader("X-Total-Size", base::NumberToString(training_data_.size()));

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
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request), traffic_annotation);
  url_loader_->AttachStringForUpload(chunk, "application/octet-stream");

  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerUploadTrainingDataFunction::OnChunkUploaded,
                     base::Unretained(this)));
}

void ReadServerUploadTrainingDataFunction::OnChunkUploaded(
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

void ReadServerUploadTrainingDataFunction::RespondWithError(
    const std::string& error_message) {
  Respond(Error(error_message));

  // Decrement reference count.
  Release();
}

void ReadServerUploadTrainingDataFunction::RespondWithSuccess() {
  Respond(WithArguments("Synthetic training data uploaded successfully"));

  // Decrement reference count.
  Release();
}


// Training on MNIST
ReadServerTrainModelFunction::ReadServerTrainModelFunction() = default;

ReadServerTrainModelFunction::~ReadServerTrainModelFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "TrainModel function destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerTrainModelFunction::Run() {
  LOG(INFO) << "ReadServerTrainModelFunction::Run() called";
  AddRef();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/train_model");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  // In this simple example, no payload is required, so we send an empty JSON.
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
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

  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerTrainModelFunction::OnTrainModelResponse, base::Unretained(this)));

  return RespondLater();
}

void ReadServerTrainModelFunction::OnTrainModelResponse(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Training request failed."));
  } else {
    Respond(WithArguments(*response_body));
  }
  Release();
}

// Constructor
ReadServerInferenceFunction::ReadServerInferenceFunction() = default;

// Destructor
ReadServerInferenceFunction::~ReadServerInferenceFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "Function was destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

// Run method to send POST request to /infer endpoint
ExtensionFunction::ResponseAction ReadServerInferenceFunction::Run() {
  LOG(INFO) << "ReadServerInferenceFunction::Run() called";

  // Validate that arguments exist and the first argument is a string
  EXTENSION_FUNCTION_VALIDATE(args().size() > 0);
  const base::Value& arg = args()[0];
  EXTENSION_FUNCTION_VALIDATE(arg.is_string());

  const std::string& features = arg.GetString();

  // Increment reference count to keep the function alive until response is received
  AddRef();

  // Define the request to the /infer endpoint
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/infer");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  // Convert features string to JSON format for the request body
  std::string json_data = "{\"features\": \"" + features + "\"}";

  // Initialize SimpleURLLoader for sending the request
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request), net::DefineNetworkTrafficAnnotation("inference_request", R"(
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

  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerInferenceFunction::OnInferenceResponse, base::Unretained(this)));

  return RespondLater();
}

// Callback to handle the response from the /infer endpoint
void ReadServerInferenceFunction::OnInferenceResponse(std::unique_ptr<std::string> response_body) {
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
ReadServerLoadModelBERTFunction::ReadServerLoadModelBERTFunction() = default;
ReadServerLoadModelBERTFunction::~ReadServerLoadModelBERTFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "LoadModelBERT function destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerLoadModelBERTFunction::Run() {
  LOG(INFO) << "ReadServerLoadModelBERTFunction::Run() called";
  AddRef();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/load_model_bert");
  resource_request->method = "POST";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("load_model_bert", R"(
        semantics {
          sender: "Read Server API"
          description: "Loads the MobileBERT model on the local server."
          trigger: "User action in the extension."
          data: "No user data is sent."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be disabled by settings."
        }
      )");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request), traffic_annotation);
  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerLoadModelBERTFunction::OnResponse, weak_ptr_factory_.GetWeakPtr()));
  return RespondLater();
}

void ReadServerLoadModelBERTFunction::OnResponse(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Failed to load MobileBERT model on server."));
  } else {
    Respond(WithArguments(*response_body));
  }
  Release();
}

// -------------------------
// Single Inference BERT Endpoint
// -------------------------
ReadServerInferSingleBERTFunction::ReadServerInferSingleBERTFunction() = default;
ReadServerInferSingleBERTFunction::~ReadServerInferSingleBERTFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "InferSingleBERT function destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerInferSingleBERTFunction::Run() {
  LOG(INFO) << "ReadServerInferSingleBERTFunction::Run() called";

  // Validate that we have at least one argument.
  EXTENSION_FUNCTION_VALIDATE(args().size() > 0);

  // The IDL defines the parameter as a DOMString, so extract it as a string.
  const std::string& json_input = args()[0].GetString();

  // Optional: Parse the JSON string to verify it represents a dictionary.
  auto maybe_value = base::JSONReader::Read(json_input);
  if (!maybe_value.has_value() || !maybe_value->is_dict()) {
    return RespondNow(Error("Input must be a JSON string representing an object with 'question' and 'context'."));
  }

  // We assume the input JSON is already in the format the server expects:
  // { "question": "…", "context": "…" }
  // So we simply forward it.
  std::string json_data = json_input;

  AddRef();
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/infer_single_bert");
  resource_request->method = "POST";
  resource_request->headers.SetHeader("Content-Type", "application/json");

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("infer_single_bert", R"(
        semantics {
          sender: "Read Server API"
          description: "Requests single-sample inference from MobileBERT."
          trigger: "User action in the extension."
          data: "A JSON object with 'question' and 'context'."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be disabled by settings."
        }
  )");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request), traffic_annotation);
  url_loader_->AttachStringForUpload(json_data, "application/json");

  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerInferSingleBERTFunction::OnResponse, base::Unretained(this)));

  return RespondLater();
}


void ReadServerInferSingleBERTFunction::OnResponse(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Single inference request failed."));
  } else {
    Respond(WithArguments(*response_body));
  }
  Release();
}

// -------------------------
// Batch Inference BERT Endpoint
// -------------------------
ReadServerInferBatchBERTFunction::ReadServerInferBatchBERTFunction() = default;
ReadServerInferBatchBERTFunction::~ReadServerInferBatchBERTFunction() {
  if (!did_respond()) {
    LOG(ERROR) << "InferBatchBERT function destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction ReadServerInferBatchBERTFunction::Run() {
  LOG(INFO) << "ReadServerInferBatchBERTFunction::Run() called";
  
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
  
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request), traffic_annotation);
  url_loader_->AttachStringForUpload(json_data, "application/json");
  
  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerInferBatchBERTFunction::OnResponse, base::Unretained(this)));
  return RespondLater();
}

void ReadServerInferBatchBERTFunction::OnResponse(std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    Respond(Error("Batch inference request failed."));
  } else {
    Respond(WithArguments(*response_body));
  }
  Release();
}

}  // namespace extensions
