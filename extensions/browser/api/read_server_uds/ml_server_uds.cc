#include "extensions/browser/api/read_server_uds/ml_server_uds.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/socket/unix_domain_client_socket_posix.h"

namespace extensions {

MLServerUDS::MLServerUDS(const std::string& socket_path,
                         const std::string& label)
    : socket_path_(socket_path), label_(label), weak_ptr_factory_(this) {}

MLServerUDS::~MLServerUDS() {
  LOG(INFO) << "MLServerUDS destroyed";
  DCHECK(!socket_) << "Socket must be cleared before destruction!";
}

void MLServerUDS::Send(const std::string& message,
                       base::OnceCallback<void(std::string)> success_cb,
                       base::OnceCallback<void(std::string)> error_cb) {
  payload_ = MLServerUDS::CreateJSONStringPayload(label_, "SEND", message);
  success_callback_ = std::move(success_cb);
  error_callback_ = std::move(error_cb);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MLServerUDS::ConnectToUnixSocket,
                                weak_ptr_factory_.GetWeakPtr()));
}

void MLServerUDS::Clear() {
  LOG(INFO) << "MLServerUDS::Clear() called";

  read_buffer_ = nullptr;
  payload_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (socket_) {
    auto temp_socket = std::move(socket_);
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<net::UnixDomainClientSocket> s) {
                         // Socket safely destroyed on IO thread
                       },
                       std::move(temp_socket)));
  }

  success_callback_.Reset();
  error_callback_.Reset();

  LOG(INFO) << "MLServerUDS internal state cleared";
}

void MLServerUDS::ConnectToUnixSocket() {
  base::FilePath path(socket_path_);
  LOG(INFO) << "Creating UnixDomainClientSocket to path: " << path.value();

  socket_ = std::make_unique<net::UnixDomainClientSocket>(
      path.value(), false /* use_abstract_namespace */);

  int result = socket_->Connect(base::BindOnce(&MLServerUDS::OnConnected,
                                               weak_ptr_factory_.GetWeakPtr()));

  if (result == net::OK) {
    LOG(INFO) << "OnConnected() synchronously";
    OnConnected(result);
  } else if (result == net::ERR_IO_PENDING) {
    LOG(INFO) << "Connection pending, waiting for callback";
  } else {
    LOG(ERROR) << "Connect failed: " << result;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback_), "Connect failed"));
  }
}

void MLServerUDS::OnConnected(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "Socket connection failed: " << result;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(error_callback_), "Socket connection failed"));
    return;
  }

  LOG(INFO) << "Socket connected, sending: " << payload_;

  auto buffer = base::MakeRefCounted<net::StringIOBuffer>(payload_);

  net::NetworkTrafficAnnotationTag annotation =
      net::DefineNetworkTrafficAnnotation("ml_server_uds_write", R"(
        semantics {
          sender: "MLServerUDS"
          description: "Sends a message to a local UDS server."
          trigger: "Extension request."
          data: "Arbitrary payload string."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This cannot be disabled in settings."
        })");

  int write_result =
      socket_->Write(buffer.get(), payload_.size(),
                     base::BindOnce(&MLServerUDS::OnDataWritten,
                                    weak_ptr_factory_.GetWeakPtr()),
                     annotation);

  if (write_result == static_cast<int>(payload_.size())) {
    LOG(INFO) << "Write synchronous";
    OnDataWritten(write_result);
  } else if (write_result == net::ERR_IO_PENDING) {
    LOG(INFO) << "Write pending";
  } else {
    LOG(ERROR) << "Write failed: " << write_result;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback_), "Write failed"));
  }
}

void MLServerUDS::OnDataWritten(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Write error: " << result;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback_), "Write failed"));
    return;
  }

  LOG(INFO) << "Write successful: " << result << " bytes";

  read_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(4096);

  int read_result = socket_->Read(
      read_buffer_.get(), 4096,
      base::BindOnce(&MLServerUDS::OnDataRead, weak_ptr_factory_.GetWeakPtr()));

  if (read_result != net::ERR_IO_PENDING && read_result <= 0) {
    LOG(ERROR) << "Read error: " << read_result;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback_), "Read failed"));
  } else {
    LOG(INFO) << "Read started";
  }
}

void MLServerUDS::OnDataRead(int result) {
  if (result <= 0) {
    LOG(ERROR) << "Read failed: " << result;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback_), "Read failed"));
    return;
  }

  std::string response(read_buffer_->data(), result);
  LOG(INFO) << "Received: " << response;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(success_callback_), std::move(response)));
}

std::string MLServerUDS::CreateJSONStringPayload(const std::string& label,
                                                 const std::string& method,
                                                 const std::string& message) {
  base::Value::Dict dict;
  dict.Set("label", label);
  dict.Set("method", method);
  dict.Set("payload", message);

  std::string json_str;
  base::JSONWriter::Write(dict, &json_str);
  return json_str;
}
}  // namespace extensions
