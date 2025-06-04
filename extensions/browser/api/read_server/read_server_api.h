#ifndef EXTENSIONS_BROWSER_API_READ_SERVER_READ_SERVER_API_H_
#define EXTENSIONS_BROWSER_API_READ_SERVER_READ_SERVER_API_H_

#include "extensions/browser/extension_function.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include <memory>
#include <string>

namespace extensions {

class ReadServerReadDataFunction : public ExtensionFunction {
public:
  DECLARE_EXTENSION_FUNCTION("readServer.readData", READSERVER_READDATA)

  ReadServerReadDataFunction();
protected:
  ~ReadServerReadDataFunction() override;
private:
  ResponseAction Run() override;
  void OnJsonLoaded(std::unique_ptr<std::string> response_body);
  void OnResponded() override;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::WeakPtrFactory<ReadServerReadDataFunction> weak_ptr_factory_{this};
};

class ReadServerSendDataFunction : public ExtensionFunction {
public:
  DECLARE_EXTENSION_FUNCTION("readServer.sendData", READSERVER_SENDDATA)

  ReadServerSendDataFunction();
protected:
  ~ReadServerSendDataFunction() override;
private:
  ResponseAction Run() override;
  void OnDataSent(std::unique_ptr<std::string> response_body);
  void OnResponded() override;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::WeakPtrFactory<ReadServerSendDataFunction> weak_ptr_factory_{this};
};

class ReadServerUploadTrainingDataFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readServer.uploadTrainingData",
                             READSERVER_UPLOADTRAININGDATA)

  ReadServerUploadTrainingDataFunction();

 protected:
  ~ReadServerUploadTrainingDataFunction() override;

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

class ReadServerTrainModelFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readServer.trainModel", READSERVER_TRAINMODEL)
  
  ReadServerTrainModelFunction();
 protected:
  ~ReadServerTrainModelFunction() override;

 private:
  ResponseAction Run() override;
  void OnTrainModelResponse(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};


class ReadServerInferenceFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readServer.inference", READSERVER_INFERENCE)  

  ReadServerInferenceFunction();
 protected:
  ~ReadServerInferenceFunction() override;

 private:
  ResponseAction Run() override;
  void OnInferenceResponse(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

// New API for loading MobileBERT model.
class ReadServerLoadModelBERTFunction : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("readServer.loadModelBERT", READSERVER_LOADMODEL_BERT)
   ReadServerLoadModelBERTFunction();
  protected:
   ~ReadServerLoadModelBERTFunction() override;
  private:
   ResponseAction Run() override;
   void OnResponse(std::unique_ptr<std::string> response_body);
   std::unique_ptr<network::SimpleURLLoader> url_loader_;
   base::WeakPtrFactory<ReadServerLoadModelBERTFunction> weak_ptr_factory_{this};
 };
 
 // New API for single inference.
 class ReadServerInferSingleBERTFunction : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("readServer.inferSingleBERT", READSERVER_INFER_SINGLE_BERT)
   ReadServerInferSingleBERTFunction();
  protected:
   ~ReadServerInferSingleBERTFunction() override;
  private:
   ResponseAction Run() override;
   void OnResponse(std::unique_ptr<std::string> response_body);
   std::unique_ptr<network::SimpleURLLoader> url_loader_;
 };
 
 // New API for batch inference.
 class ReadServerInferBatchBERTFunction : public ExtensionFunction {
  public:
   DECLARE_EXTENSION_FUNCTION("readServer.inferBatchBERT", READSERVER_INFER_BATCH_BERT)
   ReadServerInferBatchBERTFunction();
  protected:
   ~ReadServerInferBatchBERTFunction() override;
  private:
   ResponseAction Run() override;
   void OnResponse(std::unique_ptr<std::string> response_body);
   std::unique_ptr<network::SimpleURLLoader> url_loader_;
 };


}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_READ_SERVER_READ_SERVER_API_H_
