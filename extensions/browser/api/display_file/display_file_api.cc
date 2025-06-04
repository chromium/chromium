// #include "extensions/browser/api/display_file/display_file_api.h"
// #include "base/files/file_util.h"
// #include "base/feature_list.h"
// #include "content/public/common/content_features.h"
// #include "base/values.h"
// #include "base/functional/bind.h"
// #include "base/path_service.h"
// #include "base/task/single_thread_task_runner.h"
// #include "content/public/browser/browser_thread.h"
// #include "base/task/thread_pool.h"
// #include "base/task/task_traits.h"
// #include "base/task/task_runner.h"  

// namespace extensions {

// ExtensionFunction::ResponseAction DisplayFileReadFileFunction::Run() {
//     if (!base::FeatureList::IsEnabled(features::kDisplayFileContentFeature)) {
//         return RespondNow(Error("Feature not enabled"));
//     }
//     EXTENSION_FUNCTION_VALIDATE(args().size() == 0);
//     base::FilePath file_path("/home/saurav/MTP/DisplayFile/readthis.txt");
//     std::string content;
//     if (!base::ReadFileToString(file_path, &content)) {
//         return RespondNow(Error("Failed to read from the file"));
//     }

//     if (content.empty()) {
//         return RespondNow(Error("File is empty"));
//     } else {
//         return RespondNow(WithArguments(base::Value(std::move(content))));
//     }
// }

// }  // namespace extensions

// #include "extensions/browser/api/display_file/display_file_api.h"
// #include "base/files/file_util.h"
// #include "base/task/thread_pool.h"
// #include "base/functional/bind.h"
// #include "base/values.h"

// namespace extensions {

// DisplayFileReadFileFunction::DisplayFileReadFileFunction() = default;
// DisplayFileReadFileFunction::~DisplayFileReadFileFunction() = default;

// ExtensionFunction::ResponseAction DisplayFileReadFileFunction::Run() {
//   const base::FilePath file_path("/home/saurav/MTP/DisplayFile/readthis.txt");

//   base::ThreadPool::PostTaskAndReplyWithResult(
//       FROM_HERE,
//       {base::MayBlock()},
//       base::BindOnce(&DisplayFileReadFileFunction::ReadFileContents, file_path),
//       base::BindOnce(&DisplayFileReadFileFunction::RespondWithResult, this));

//   return RespondLater();
// }

// std::string DisplayFileReadFileFunction::ReadFileContents(const base::FilePath& file_path) {
//   std::string content;
//   base::ReadFileToString(file_path, &content);
//   return content;
// }

// void DisplayFileReadFileFunction::RespondWithResult(const std::string& content) {
//   if (content.empty()) {
//     Respond(Error("File is empty or could not be read"));
//   } else {
//     Respond(WithArguments(base::Value(std::move(content))));
//   }
// }

// }  // namespace extensions

#include "extensions/browser/api/display_file/display_file_api.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace extensions {

DisplayFileReadFileFunction::DisplayFileReadFileFunction() = default;

DisplayFileReadFileFunction::~DisplayFileReadFileFunction() {
  // Ensure Respond is called before destruction
  if (!did_respond()) {
    LOG(ERROR) << "Function was destroyed without responding";
    Respond(Error("Function was destroyed without responding"));
  }
}

ExtensionFunction::ResponseAction DisplayFileReadFileFunction::Run() {
  LOG(INFO) << "DisplayFileReadFileFunction::Run() called";

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL("http://localhost:5000/data");
  resource_request->method = "GET";

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("display_file_read_file", R"(
        semantics {
          sender: "Display File API"
          description: "Fetches JSON data from a local Flask server."
          trigger: "User action in the extension."
          data: "No user data is sent."
          destination: LOCAL
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be disabled by settings."
          chrome_policy {
            DisplayFileReadFile {
                DisplayFileReadFile: true
            }
          }
          policy_exception_justification: "Not implemented, as this is a local request."
        }
        comments: "This request is made to fetch JSON data from a Flask server running on the local machine."
      )");

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request), traffic_annotation);

  content::StoragePartition* storage_partition = browser_context()->GetDefaultStoragePartition();
  LOG(INFO) << "Starting network request to Flask server";

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&DisplayFileReadFileFunction::OnJsonLoaded, weak_ptr_factory_.GetWeakPtr()));  // Use GetWeakPtr() for safe callback

  LOG(INFO) << "DisplayFileReadFileFunction::Run() completed, request started";

  return RespondLater();
}

void DisplayFileReadFileFunction::OnJsonLoaded(std::unique_ptr<std::string> response_body) {
  LOG(INFO) << "DisplayFileReadFileFunction::OnJsonLoaded() called";

  if (!response_body) {
    LOG(ERROR) << "Failed to load JSON from server: response_body is null";
    Respond(Error("Failed to load JSON from server"));
    return;
  }

  LOG(INFO) << "Response body: " << *response_body;

  auto json = base::JSONReader::Read(*response_body);
  if (!json.has_value()) {
    LOG(ERROR) << "Failed to parse JSON response";
    Respond(Error("Failed to parse JSON response"));
    return;
  }

  LOG(INFO) << "JSON loaded and parsed successfully";
  Respond(WithArguments(std::move(*json)));
}

}  // namespace extensions








