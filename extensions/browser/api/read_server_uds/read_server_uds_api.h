#ifndef EXTENSIONS_BROWSER_API_READ_SERVER_UDS_API_H_
#define EXTENSIONS_BROWSER_API_READ_SERVER_UDS_API_H_

#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_function.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "net/base/io_buffer.h"
#include "net/socket/unix_domain_client_socket_posix.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#include <memory>

#include <string>

namespace extensions {

// send data
class ReadServerUdsReadDataFunction : public ExtensionFunction {
public:
  DECLARE_EXTENSION_FUNCTION("readServerUds.readData", READSERVERUDS_READDATA)

  ReadServerUdsReadDataFunction();

protected:
  ~ReadServerUdsReadDataFunction() override;

private:
  ResponseAction Run() override;
  void OnResponded() override;

  // Socket handling
  void ConnectToUnixSocket();
  void OnConnected(int result);
  void OnDataWritten(int result);
  void OnDataRead(int result);
  void RespondSuccessOnUI(std::string json_result);
  void RespondFromIOThread(ResponseValue error_result);
  void RespondErrorOnUI(base::Value error_result);

  std::unique_ptr<net::UnixDomainClientSocket> socket_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  base::WeakPtrFactory<ReadServerUdsReadDataFunction> weak_ptr_factory_{this};
};



// send data by chunking
class ReadServerUdsSendDataFunction : public ExtensionFunction {
public:
  DECLARE_EXTENSION_FUNCTION("readServerUds.sendData", READSERVERUDS_SENDDATA)

  ReadServerUdsSendDataFunction();
protected:
  ~ReadServerUdsSendDataFunction() override;
private:
  ResponseAction Run() override;
  void OnResponded() override;

  // handling function
  void ConnectToUnixSocket();
  void OnConnected(int result);
  void OnDataWritten(int result);
  void OnDataRead(int result);
  void RespondSuccessOnUI(std::string json_result);
  void RespondFromIOThread(ResponseValue error_result);
  void RespondErrorOnUI(base::Value error_result);

  std::string message_;
  std::unique_ptr<net::UnixDomainClientSocket> socket_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  base::WeakPtrFactory<ReadServerUdsSendDataFunction> weak_ptr_factory_{this};
};

class ReadServerUdsUploadTrainingDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readServerUds.uploadTrainingData",
                             READSERVERUDS_UPLOADTRAININGDATA)

  ReadServerUdsUploadTrainingDataFunction();

 protected:
  ~ReadServerUdsUploadTrainingDataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Methods
  void GenerateSyntheticData();
  void StartUploadingData();
  void UploadNextChunk();
  void OnChunkUploaded(std::unique_ptr<std::string> response_body);
  void RespondWithError(const std::string& error_message);
  void RespondWithSuccess();

  // Member variables
  std::string training_data_;
  size_t chunk_size_;
  size_t offset_;

  // Declare url_loader_
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

};

class ReadServerUdsTrainModelFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readServerUds.trainModel", READSERVERUDS_TRAINMODEL)
  
  ReadServerUdsTrainModelFunction();
 protected:
  ~ReadServerUdsTrainModelFunction() override;

 private:
  ResponseAction Run() override;
  void OnTrainModelResponse(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};


class ReadServerUdsInferenceFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readServerUds.inference", READSERVERUDS_INFERENCE)

  ReadServerUdsInferenceFunction();
 protected:
  ~ReadServerUdsInferenceFunction() override;

 private:
  ResponseAction Run() override;
  void OnInferenceResponse(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

// New API for loading MobileBERT model.
class ReadServerUdsLoadModelBERTFunction : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("readServerUds.loadModelBERT", READSERVERUDS_LOADMODEL_BERT)
   ReadServerUdsLoadModelBERTFunction();
  protected:
   ~ReadServerUdsLoadModelBERTFunction() override;
  private:
   ResponseAction Run() override;
   void OnResponse(std::unique_ptr<std::string> response_body);
   std::unique_ptr<network::SimpleURLLoader> url_loader_;
   base::WeakPtrFactory<ReadServerUdsLoadModelBERTFunction> weak_ptr_factory_{this};
 };
 
 // New API for single inference.
 class ReadServerUdsInferSingleBERTFunction : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("readServerUds.inferSingleBERT", READSERVERUDS_INFER_SINGLE_BERT)
   ReadServerUdsInferSingleBERTFunction();
  protected:
   ~ReadServerUdsInferSingleBERTFunction() override;
  private:
   ResponseAction Run() override;
   void OnResponse(std::unique_ptr<std::string> response_body);
   std::unique_ptr<network::SimpleURLLoader> url_loader_;
 };
 
 // New API for batch inference.
 class ReadServerUdsInferBatchBERTFunction : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("readServerUds.inferBatchBERT", READSERVERUDS_INFER_BATCH_BERT)
   ReadServerUdsInferBatchBERTFunction();
  protected:
   ~ReadServerUdsInferBatchBERTFunction() override;
  private:
   ResponseAction Run() override;
   void OnResponse(std::unique_ptr<std::string> response_body);
   std::unique_ptr<network::SimpleURLLoader> url_loader_;
 };


}  // namespace extensions

#endif // EXTENSIONS_BROWSER_API_READ_SERVER_UDS_API_H_