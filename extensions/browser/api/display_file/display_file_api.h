// #ifndef EXTENSIONS_BROWSER_API_DISPLAY_FILE_DISPLAY_FILE_API_H_
// #define EXTENSIONS_BROWSER_API_DISPLAY_FILE_DISPLAY_FILE_API_H_

// #include "extensions/browser/extension_function.h"
// #include "base/files/file_path.h"
// #include <string>  

// namespace extensions {

// class DisplayFileReadFileFunction : public ExtensionFunction {
//  public:
//   DECLARE_EXTENSION_FUNCTION("displayFile.readFile", DISPLAYFILE_READFILE)

//   DisplayFileReadFileFunction();
//   DisplayFileReadFileFunction(const DisplayFileReadFileFunction&) = delete;
//   DisplayFileReadFileFunction& operator=(const DisplayFileReadFileFunction&) = delete;

//  protected:
//   ~DisplayFileReadFileFunction() override;  

//  private:
//   ResponseAction Run() override;
//   static std::string ReadFileContents(const base::FilePath& file_path);
//   void RespondWithResult(const std::string& content);
// };

// }  // namespace extensions

// #endif  // EXTENSIONS_BROWSER_API_DISPLAY_FILE_DISPLAY_FILE_API_H_

#ifndef EXTENSIONS_BROWSER_API_DISPLAY_FILE_DISPLAY_FILE_API_H_
#define EXTENSIONS_BROWSER_API_DISPLAY_FILE_DISPLAY_FILE_API_H_

#include "extensions/browser/extension_function.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include <memory>
#include <string>

namespace extensions {

class DisplayFileReadFileFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("displayFile.readFile", DISPLAYFILE_READFILE)
  
  DisplayFileReadFileFunction();
  DisplayFileReadFileFunction(const DisplayFileReadFileFunction&) = delete;
  DisplayFileReadFileFunction& operator=(const DisplayFileReadFileFunction&) = delete;

 protected:
  ~DisplayFileReadFileFunction() override;

 private:
  ResponseAction Run() override;
  void OnJsonLoaded(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  base::WeakPtrFactory<DisplayFileReadFileFunction> weak_ptr_factory_{this};  // Using WeakPtrFactory for safe callbacks
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DISPLAY_FILE_DISPLAY_FILE_API_H_
