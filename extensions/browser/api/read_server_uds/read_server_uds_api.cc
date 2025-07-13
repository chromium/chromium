#include "extensions/browser/api/read_server_uds/read_server_uds_api.h"

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
}  // namespace extensions
