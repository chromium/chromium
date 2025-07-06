#include "extensions/browser/api/read_server_uds/read_server_uds_api.h"

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

// this the base function when the extension api is called
ExtensionFunction::ResponseAction ReadServerUdsReadDataFunction::Run() {
  if (!render_frame_host()) {
    return RespondNow(Error("Invalid frame"));
  }

  // for async task
  AddRef();

  // Post the Connect operation to IO thread
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadServerUdsReadDataFunction::ConnectToUnixSocket,
                     weak_ptr_factory_.GetWeakPtr()));

  return RespondLater();
}

void ReadServerUdsReadDataFunction::ConnectToUnixSocket() {

  base::FilePath socket_path("/tmp/shared-sockets/echo_socket");

  LOG(INFO) << "Creating UnixDomainClientSocket to path: " << socket_path.value();

  socket_ = std::make_unique<net::UnixDomainClientSocket>(
      socket_path.value(), false /* use_abstract_namespace */);

  // this is uds so connect will be synchronous
  int result = socket_->Connect(
  base::BindOnce(&ReadServerUdsReadDataFunction::OnConnected,
                  weak_ptr_factory_.GetWeakPtr()));

  LOG(INFO) << "Connect() returned: " << result;
  if (result == net::OK) {
    LOG(INFO) << "OnConnected() synchronously";
    OnConnected(result);
  } else if (result == net::ERR_IO_PENDING) {
    LOG(INFO) << "Connection pending, waiting for callback";
  } else {
    LOG(ERROR) << "Connect failed: " << result;
    RespondFromIOThread(Error("Connect failed"));
  }
}

void ReadServerUdsReadDataFunction::OnConnected(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "OnConnected: Socket connection failed: " << result;
    RespondFromIOThread(Error("Socket connection failed"));
    return;
  }

  LOG(INFO) << "Socket connected successfully";

  std::string message = "GET /data\n";
  LOG(INFO) << "Sending message: " << message;

  auto send_buffer = base::MakeRefCounted<net::StringIOBuffer>(message);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("read_server_uds_write", R"(
      semantics {
        sender: "Read Server UDS API"
        description: "Sends a message to the UNIX domain socket server."
        trigger: "User action in the extension."
        data: "A command string."
        destination: LOCAL
      }
      policy {
        cookies_allowed: NO
        setting: "This request cannot be disabled by settings."
      })");


  // the no of byte written return by the write()
  // describe at net/socket/socket.h:74
  int write_result = socket_->Write(
      send_buffer.get(), message.size(),
      base::BindOnce(&ReadServerUdsReadDataFunction::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr()),
      traffic_annotation);
  
  if (write_result == static_cast<int>(message.size())) { // synchronous call
    LOG(INFO) << "OnDataWritten() Synchronous";
    OnDataWritten(write_result);
    LOG(INFO) << "Data written to socket immediately";
  } else if (write_result == net::ERR_IO_PENDING) { // async 
    LOG(INFO) << "Write to socket pending";
  } else {
    LOG(ERROR) << "Failed to write to socket: " << write_result;
    RespondFromIOThread(Error("Failed to write to socket"));
  }
}

void ReadServerUdsReadDataFunction::OnDataWritten(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Failed to write data: " << result;
    RespondFromIOThread(Error("Write failed"));
    return;
  }

  LOG(INFO) << "Data written successfully, bytes: " << result;

  read_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(4096);

  // refere to the net/socket/socket.h:38
  int read_result = socket_->Read(
      read_buffer_.get(), 4096,
      base::BindOnce(&ReadServerUdsReadDataFunction::OnDataRead,
                     weak_ptr_factory_.GetWeakPtr()));

  if (read_result != net::ERR_IO_PENDING && read_result <= 0) {
    LOG(ERROR) << "Read failed: " << read_result;
    RespondFromIOThread(Error("Read failed"));
  } else { // 
    LOG(INFO) << "Read started";
  }
}

void ReadServerUdsReadDataFunction::OnDataRead(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Failed to read from socket: " << result;
    RespondFromIOThread(Error("Failed to read from socket"));
    return;
  }

  std::string response(read_buffer_->data(), result);
  LOG(INFO) << "Response data: " << response;

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  LOG(INFO) << "Inside OnDataRead() on IO thread";

  content::GetUIThreadTaskRunner({})->PostTask(
    FROM_HERE,
    base::BindOnce(&ReadServerUdsReadDataFunction::RespondSuccessOnUI,
                   base::Unretained(this), std::move(response)));
}

void ReadServerUdsReadDataFunction::RespondSuccessOnUI(std::string result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  LOG(INFO) << "Inside RespondSuccessOnUI() on UI thread";

  Respond(WithArguments(base::Value(result)));
  Release();
}

void ReadServerUdsReadDataFunction::RespondFromIOThread(ResponseValue result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  LOG(INFO) << "Inside RespondFromIOThread() on IO thread";
  
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadServerUdsReadDataFunction::Respond,
                     base::Unretained(this), std::move(result)));
}

void ReadServerUdsReadDataFunction::RespondErrorOnUI(base::Value error_result) {
  if (error_result.is_string()) {
    Respond(Error(error_result.GetString()));
  } else {
    Respond(WithArguments(std::move(error_result)));
  }
  Release();
}

// clean up 
// called by the extension automatcially when send response
void ReadServerUdsReadDataFunction::OnResponded() {
  LOG(INFO) << "Cleaning up socket";

  // Post socket cleanup to the IO thread
  if (socket_) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<net::UnixDomainClientSocket> socket) {
              // Socket will be destructed here, safely on IO thread
            },
            std::move(socket_)));
  }
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

  // Extract the message from the arguments and save it in this class variable
  message_ = arg.GetString();
  LOG(INFO) << "Sending message: " << message_;

  AddRef();  // Keep the function alive until the request is completed

    // Post the Connect operation to IO thread
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadServerUdsSendDataFunction::ConnectToUnixSocket,
                     weak_ptr_factory_.GetWeakPtr()));

  LOG(INFO)
      << "ReadServerUdsSendDataFunction::Run() completed, request started";
  return RespondLater();
}

void ReadServerUdsSendDataFunction::ConnectToUnixSocket() {

  base::FilePath socket_path("/tmp/shared-sockets/echo_socket");

  LOG(INFO) << "Creating UnixDomainClientSocket to path: " << socket_path.value();

  socket_ = std::make_unique<net::UnixDomainClientSocket>(
      socket_path.value(), false /* use_abstract_namespace */);

  // this is uds so connect will be synchronous
  int result = socket_->Connect(
  base::BindOnce(&ReadServerUdsSendDataFunction::OnConnected,
                  weak_ptr_factory_.GetWeakPtr()));

  LOG(INFO) << "Connect() returned: " << result;
  if (result == net::OK) {
    LOG(INFO) << "OnConnected() synchronously";
    OnConnected(result);
  } else if (result == net::ERR_IO_PENDING) {
    LOG(INFO) << "Connection pending, waiting for callback";
  } else {
    LOG(ERROR) << "Connect failed: " << result;
    RespondFromIOThread(Error("Connect failed"));
  }
}

void ReadServerUdsSendDataFunction::OnConnected(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "OnConnected: Socket connection failed: " << result;
    RespondFromIOThread(Error("Socket connection failed"));
    return;
  }

  LOG(INFO) << "Socket connected successfully";

  LOG(INFO) << "Sending message: " << message_;

  auto send_buffer = base::MakeRefCounted<net::StringIOBuffer>(message_);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("read_server_uds_write", R"(
      semantics {
        sender: "Read Server UDS API"
        description: "Sends a message to the UNIX domain socket server."
        trigger: "User action in the extension."
        data: "A command string."
        destination: LOCAL
      }
      policy {
        cookies_allowed: NO
        setting: "This request cannot be disabled by settings."
      })");


  // the no of byte written return by the write()
  // describe at net/socket/socket.h:74
  int write_result = socket_->Write(
      send_buffer.get(), message_.size(),
      base::BindOnce(&ReadServerUdsSendDataFunction::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr()),
      traffic_annotation);
  
  if (write_result == static_cast<int>(message_.size())) { // synchronous call
    LOG(INFO) << "OnDataWritten() Synchronous";
    OnDataWritten(write_result);
    LOG(INFO) << "Data written to socket immediately";
  } else if (write_result == net::ERR_IO_PENDING) { // async 
    LOG(INFO) << "Write to socket pending";
  } else {
    LOG(ERROR) << "Failed to write to socket: " << write_result;
    RespondFromIOThread(Error("Failed to write to socket"));
  }
}

void ReadServerUdsSendDataFunction::OnDataWritten(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Failed to write data: " << result;
    RespondFromIOThread(Error("Write failed"));
    return;
  }

  LOG(INFO) << "Data written successfully, bytes: " << result;

  read_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(4096);

  // refere to the net/socket/socket.h:38
  int read_result = socket_->Read(
      read_buffer_.get(), 4096,
      base::BindOnce(&ReadServerUdsSendDataFunction::OnDataRead,
                     weak_ptr_factory_.GetWeakPtr()));

  if (read_result != net::ERR_IO_PENDING && read_result <= 0) {
    LOG(ERROR) << "Read failed: " << read_result;
    RespondFromIOThread(Error("Read failed"));
  } else { // 
    LOG(INFO) << "Read started";
  }
}

void ReadServerUdsSendDataFunction::OnDataRead(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Failed to read from socket: " << result;
    RespondFromIOThread(Error("Failed to read from socket"));
    return;
  }

  std::string response(read_buffer_->data(), result);
  LOG(INFO) << "Response data: " << response;

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  LOG(INFO) << "Inside OnDataRead() on IO thread";

  content::GetUIThreadTaskRunner({})->PostTask(
    FROM_HERE,
    base::BindOnce(&ReadServerUdsSendDataFunction::RespondSuccessOnUI,
                   base::Unretained(this), std::move(response)));
}

void ReadServerUdsSendDataFunction::RespondSuccessOnUI(std::string result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  LOG(INFO) << "Inside RespondSuccessOnUI() on UI thread";

  Respond(WithArguments(base::Value(result)));
  Release();
}

void ReadServerUdsSendDataFunction::RespondFromIOThread(ResponseValue result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  LOG(INFO) << "Inside RespondFromIOThread() on IO thread";
  
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReadServerUdsSendDataFunction::Respond,
                     base::Unretained(this), std::move(result)));
}

void ReadServerUdsSendDataFunction::RespondErrorOnUI(base::Value error_result) {
  if (error_result.is_string()) {
    Respond(Error(error_result.GetString()));
  } else {
    Respond(WithArguments(std::move(error_result)));
  }
  Release();
}

// clean up 
// called by the extension automatcially when send response
void ReadServerUdsSendDataFunction::OnResponded() {
  LOG(INFO) << "Cleaning up socket";

  // Post socket cleanup to the IO thread
  if (socket_) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<net::UnixDomainClientSocket> socket) {
              // Socket will be destructed here, safely on IO thread
            },
            std::move(socket_)));
  }
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

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  content::StoragePartition* storage_partition =
      browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerUdsLoadModelBERTFunction::OnResponse,
                     weak_ptr_factory_.GetWeakPtr()));
  return RespondLater();
}

void ReadServerUdsLoadModelBERTFunction::OnResponse(
    std::unique_ptr<std::string> response_body) {
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

  // Validate that we have at least one argument.
  EXTENSION_FUNCTION_VALIDATE(args().size() > 0);

  // The IDL defines the parameter as a DOMString, so extract it as a string.
  const std::string& json_input = args()[0].GetString();

  // Optional: Parse the JSON string to verify it represents a dictionary.
  auto maybe_value = base::JSONReader::Read(json_input);
  if (!maybe_value.has_value() || !maybe_value->is_dict()) {
    return RespondNow(
        Error("Input must be a JSON string representing an object with "
              "'question' and 'context'."));
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

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->AttachStringForUpload(json_data, "application/json");

  content::StoragePartition* storage_partition =
      browser_context()->GetDefaultStoragePartition();
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&ReadServerUdsInferSingleBERTFunction::OnResponse,
                     base::Unretained(this)));

  return RespondLater();
}

void ReadServerUdsInferSingleBERTFunction::OnResponse(
    std::unique_ptr<std::string> response_body) {
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
