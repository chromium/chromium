#ifndef EXTENSIONS_BROWSER_API_READ_SERVER_UDS_API_H_
#define EXTENSIONS_BROWSER_API_READ_SERVER_UDS_API_H_
#include "extensions/browser/api/read_server_uds/ml_server_uds.h"

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_function.h"

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
  void OnSuccess(std::string result);
  void OnError(std::string error_msg);

  std::unique_ptr<extensions::MLServerUDS> ml_server_;
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

  void OnSuccess(std::string result);
  void OnError(std::string error_msg);

  std::unique_ptr<extensions::MLServerUDS> ml_server_;
  base::WeakPtrFactory<ReadServerUdsSendDataFunction> weak_ptr_factory_{this};
};

// New API for loading MobileBERT model.
class ReadServerUdsLoadModelBERTFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readServerUds.loadModelBERT",
                             READSERVERUDS_LOADMODEL_BERT)
  ReadServerUdsLoadModelBERTFunction();

 protected:
  ~ReadServerUdsLoadModelBERTFunction() override;

 private:
  ResponseAction Run() override;
  void OnResponded() override;

  // Socket handling
  void OnSuccess(std::string result);
  void OnError(std::string error_msg);

  std::unique_ptr<extensions::MLServerUDS> ml_server_;
  base::WeakPtrFactory<ReadServerUdsLoadModelBERTFunction> weak_ptr_factory_{
      this};
};

// New API for single inference.
class ReadServerUdsInferSingleBERTFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("readServerUds.inferSingleBERT",
                             READSERVERUDS_INFER_SINGLE_BERT)
  ReadServerUdsInferSingleBERTFunction();

 protected:
  ~ReadServerUdsInferSingleBERTFunction() override;

 private:
  ResponseAction Run() override;
  void OnResponded() override;

  // Socket handling
  void OnSuccess(std::string result);
  void OnError(std::string error_msg);

  std::unique_ptr<extensions::MLServerUDS> ml_server_;
  base::WeakPtrFactory<ReadServerUdsInferSingleBERTFunction> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_READ_SERVER_UDS_API_H_
