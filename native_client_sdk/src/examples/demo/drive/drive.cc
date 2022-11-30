// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "json/reader.h"
#include "json/writer.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/url_loader.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/url_response_info.h"
#include "ppapi/cpp/var.h"
#include "ppapi/utility/completion_callback_factory.h"
#include "ppapi/utility/threading/simple_thread.h"

namespace {

// When we upload files, we also upload the metadata at the same time. To do so,
// we use the mimetype multipart/related. This mimetype requires specifying a
// boundary between the JSON metadata and the file content.
const char kBoundary[] = "NACL_BOUNDARY_600673";

// This is a simple implementation of JavaScript's encodeUriComponent. We
// assume the data is already UTF-8. See
// https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/encodeURIComponent.
std::string EncodeUriComponent(const std::string& s) {
  char hex[] = "0123456789ABCDEF";
  std::string result;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (isalpha(c) || isdigit(c) || strchr("-_.!~*'()", c)) {
      result += c;
    } else {
      result += '%';
      result += hex[(c >> 4) & 0xf];
      result += hex[c & 0xf];
    }
  }
  return result;
}

std::string IntToString(int x) {
  char buffer[32];
  snprintf(&buffer[0], 32, "%d", x);
  return &buffer[0];
}

void AddQueryParameter(std::string* s,
                       const std::string& key,
                       const std::string& value,
                       bool first) {
  *s += first ? '?' : '&';
  *s += EncodeUriComponent(key);
  *s += '=';
  *s += EncodeUriComponent(value);
}

void AddQueryParameter(std::string* s,
                       const std::string& key,
                       int value,
                       bool first) {
  AddQueryParameter(s, key, IntToString(value), first);
}

void AddAuthTokenHeader(std::string* s, const std::string& auth_token) {
  *s += "Authorization: Bearer ";
  *s += auth_token;
  *s += "\n";
}

void AddHeader(std::string* s, const char* key, const std::string& value) {
  *s += key;
  *s += ": ";
  *s += value;
  *s += "\n";
}

}  // namespace

//
// ReadUrl
//
struct ReadUrlParams {
  std::string url;
  std::string method;
  std::string request_headers;
  std::string request_body;
};

// This function blocks so it needs to be called off the main thread.
int32_t ReadUrl(pp::Instance* instance,
                const ReadUrlParams& params,
                std::string* output) {
  pp::URLRequestInfo url_request(instance);
  pp::URLLoader url_loader(instance);

  url_request.SetURL(params.url);
  url_request.SetMethod(params.method);
  url_request.SetHeaders(params.request_headers);
  url_request.SetRecordDownloadProgress(true);
  if (params.request_body.size()) {
    url_request.AppendDataToBody(params.request_body.data(),
                                 params.request_body.size());
  }

  int32_t result = url_loader.Open(url_request, pp::BlockUntilComplete());
  if (result != PP_OK) {
    return result;
  }

  pp::URLResponseInfo url_response = url_loader.GetResponseInfo();
  if (url_response.GetStatusCode() != 200)
    return PP_ERROR_FAILED;

  output->clear();

  int64_t bytes_received = 0;
  int64_t total_bytes_to_be_received = 0;
  if (url_loader.GetDownloadProgress(&bytes_received,
                                     &total_bytes_to_be_received)) {
    if (total_bytes_to_be_received > 0) {
      output->reserve(total_bytes_to_be_received);
    }
  }

  url_request.SetRecordDownloadProgress(false);

  const int32_t kReadBufferSize = 16 * 1024;
  uint8_t* buffer_ = new uint8_t[kReadBufferSize];

  do {
    result = url_loader.ReadResponseBody(
        buffer_, kReadBufferSize, pp::BlockUntilComplete());
    if (result > 0) {
      assert(result <= kReadBufferSize);
      size_t num_bytes = result;
      output->insert(output->end(), buffer_, buffer_ + num_bytes);
    }
  } while (result > 0);

  delete[] buffer_;

  return result;
}

//
// ListFiles
//
// This is a simplistic implementation of the files.list method defined here:
// https://developers.google.com/drive/v2/reference/files/list
//
struct ListFilesParams {
  int max_results;
  std::string page_token;
  std::string query;
};

int32_t ListFiles(pp::Instance* instance,
                  const std::string& auth_token,
                  const ListFilesParams& params,
                  Json::Value* root) {
  static const char base_url[] = "https://www.googleapis.com/drive/v2/files";

  ReadUrlParams p;
  p.method = "GET";
  p.url = base_url;
  AddQueryParameter(&p.url, "maxResults", params.max_results, true);
  if (params.page_token.length())
    AddQueryParameter(&p.url, "pageToken", params.page_token, false);
  AddQueryParameter(&p.url, "q", params.query, false);
  // Request a "partial response". See
  // https://developers.google.com/drive/performance#partial for more
  // information.
  AddQueryParameter(&p.url, "fields", "items(id,downloadUrl)", false);
  AddAuthTokenHeader(&p.request_headers, auth_token);

  std::string output;
  int32_t result = ReadUrl(instance, p, &output);
  if (result != PP_OK) {
    return result;
  }

  Json::Reader reader(Json::Features::strictMode());
  if (!reader.parse(output, *root, false)) {
    return PP_ERROR_FAILED;
  }

  return PP_OK;
}

//
// InsertFile
//
// This is a simplistic implementation of the files.update and files.insert
// methods defined here:
// https://developers.google.com/drive/v2/reference/files/insert
// https://developers.google.com/drive/v2/reference/files/update
//
struct InsertFileParams {
  // If file_id is empty, create a new file (files.insert). If file_id is not
  // empty, update that file (files.update)
  std::string file_id;
  std::string content;
  std::string description;
  std::string mime_type;
  std::string title;
};

std::string BuildRequestBody(const InsertFileParams& params) {
  // This generates the multipart-upload request body for InsertFile. See
  // https://developers.google.com/drive/manage-uploads#multipart for more
  // information.
  std::string result;
  result += "--";
  result += kBoundary;
  result += "\nContent-Type: application/json; charset=UTF-8\n\n";

  Json::Value value(Json::objectValue);
  if (!params.description.empty())
    value["description"] = Json::Value(params.description);

  if (!params.mime_type.empty())
    value["mimeType"] = Json::Value(params.mime_type);

  if (!params.title.empty())
    value["title"] = Json::Value(params.title);

  Json::FastWriter writer;
  std::string metadata = writer.write(value);

  result += metadata;
  result += "--";
  result += kBoundary;
  result += "\nContent-Type: ";
  result += params.mime_type;
  result += "\n\n";
  result += params.content;
  result += "\n--";
  result += kBoundary;
  result += "--";
  return result;
}

int32_t InsertFile(pp::Instance* instance,
                   const std::string& auth_token,
                   const InsertFileParams& params,
                   Json::Value* root) {
  static const char base_url[] =
      "https://www.googleapis.com/upload/drive/v2/files";

  ReadUrlParams p;
  p.url = base_url;

  // If file_id is defined, we are actually updating an existing file.
  if (!params.file_id.empty()) {
    p.url += "/";
    p.url += params.file_id;
    p.method = "PUT";
  } else {
    p.method = "POST";
  }

  // We always use the multipart upload interface, but see
  // https://developers.google.com/drive/manage-uploads for other
  // options.
  AddQueryParameter(&p.url, "uploadType", "multipart", true);
  // Request a "partial response". See
  // https://developers.google.com/drive/performance#partial for more
  // information.
  AddQueryParameter(&p.url, "fields", "id,downloadUrl", false);
  AddAuthTokenHeader(&p.request_headers, auth_token);
  AddHeader(&p.request_headers,
            "Content-Type",
            std::string("multipart/related; boundary=") + kBoundary + "\n");
  p.request_body = BuildRequestBody(params);

  std::string output;
  int32_t result = ReadUrl(instance, p, &output);
  if (result != PP_OK) {
    return result;
  }

  Json::Reader reader(Json::Features::strictMode());
  if (!reader.parse(output, *root, false)) {
    return PP_ERROR_FAILED;
  }

  return PP_OK;
}

//
// Instance
//
class Instance : public pp::Instance {
 public:
  Instance(PP_Instance instance);
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);
  virtual void HandleMessage(const pp::Var& var_message);

  void PostMessagef(const char* format, ...);

 private:
  void ThreadSetAuthToken(int32_t, const std::string& auth_token);
  void ThreadRequestThunk(int32_t);
  bool ThreadRequest();
  bool ThreadGetFileMetadata(const char* title, Json::Value* metadata);
  bool ThreadCreateFile(const char* title,
                        const char* description,
                        const char* content,
                        Json::Value* metadata);
  bool ThreadUpdateFile(const std::string& file_id,
                        const std::string& content,
                        Json::Value* metadata);
  bool ThreadDownloadFile(const Json::Value& metadata, std::string* output);
  bool GetMetadataKey(const Json::Value& metadata,
                      const char* key,
                      std::string* output);

  pp::SimpleThread worker_thread_;
  pp::CompletionCallbackFactory<Instance> callback_factory_;
  std::string auth_token_;
  bool is_processing_request_;
};

Instance::Instance(PP_Instance instance)
    : pp::Instance(instance),
      worker_thread_(this),
      callback_factory_(this),
      is_processing_request_(false) {}

bool Instance::Init(uint32_t /*argc*/,
                    const char * [] /*argn*/,
                    const char * [] /*argv*/) {
  worker_thread_.Start();
  return true;
}

void Instance::HandleMessage(const pp::Var& var_message) {
  const char kTokenMessage[] = "token:";
  const size_t kTokenMessageLen = strlen(kTokenMessage);
  const char kGetFileMessage[] = "getFile";

  if (!var_message.is_string()) {
    return;
  }

  std::string message = var_message.AsString();
  printf("Got message: \"%s\"\n", message.c_str());
  if (message.compare(0, kTokenMessageLen, kTokenMessage) == 0) {
    // Auth token
    std::string auth_token = message.substr(kTokenMessageLen);
    worker_thread_.message_loop().PostWork(callback_factory_.NewCallback(
        &Instance::ThreadSetAuthToken, auth_token));
  } else if (message == kGetFileMessage) {
    // Request
    if (!is_processing_request_) {
      is_processing_request_ = true;
      worker_thread_.message_loop().PostWork(
          callback_factory_.NewCallback(&Instance::ThreadRequestThunk));
    }
  }
}

void Instance::PostMessagef(const char* format, ...) {
  const size_t kBufferSize = 1024;
  char buffer[kBufferSize];
  va_list args;
  va_start(args, format);
  vsnprintf(&buffer[0], kBufferSize, format, args);

  PostMessage(buffer);
}

void Instance::ThreadSetAuthToken(int32_t /*result*/,
                                  const std::string& auth_token) {
  printf("Got auth token: %s\n", auth_token.c_str());
  auth_token_ = auth_token;
}

void Instance::ThreadRequestThunk(int32_t /*result*/) {
  ThreadRequest();
  is_processing_request_ = false;
}

bool Instance::ThreadRequest() {
  static int request_count = 0;
  static const char kTitle[] = "hello nacl.txt";
  Json::Value metadata;
  std::string output;

  PostMessagef("log:\n Got request (#%d).\n", ++request_count);
  PostMessagef("log: Looking for file: \"%s\".\n", kTitle);

  if (!ThreadGetFileMetadata(kTitle, &metadata)) {
    PostMessage("log: Not found! Creating a new file...\n");
    // No data found, write a new file.
    static const char kDescription[] = "A file generated by NaCl!";
    static const char kInitialContent[] = "Hello, Google Drive!";

    if (!ThreadCreateFile(kTitle, kDescription, kInitialContent, &metadata)) {
      PostMessage("log: Creating the new file failed...\n");
      return false;
    }
  } else {
    PostMessage("log: Found it! Downloading the file...\n");
    // Found the file, download it's data.
    if (!ThreadDownloadFile(metadata, &output)) {
      PostMessage("log: Downloading the file failed...\n");
      return false;
    }

    // Modify it.
    output += "\nHello, again Google Drive!";

    std::string file_id;
    if (!GetMetadataKey(metadata, "id", &file_id)) {
      PostMessage("log: Couldn't find the file id...\n");
      return false;
    }

    PostMessage("log: Updating the file...\n");
    if (!ThreadUpdateFile(file_id, output, &metadata)) {
      PostMessage("log: Failed to update the file...\n");
      return false;
    }
  }

  PostMessage("log: Done!\n");
  PostMessage("log: Downloading the newly written file...\n");
  if (!ThreadDownloadFile(metadata, &output)) {
    PostMessage("log: Downloading the file failed...\n");
    return false;
  }

  PostMessage("log: Done!\n");
  PostMessage(output);
  return true;
}

bool Instance::ThreadGetFileMetadata(const char* title, Json::Value* metadata) {
  ListFilesParams p;
  p.max_results = 1;
  p.query = "title = \'";
  p.query += title;
  p.query += "\'";

  Json::Value root;
  int32_t result = ListFiles(this, auth_token_, p, &root);
  if (result != PP_OK) {
    PostMessagef("log: ListFiles failed with result %d\n", result);
    return false;
  }

  // Extract the first item's metadata.
  if (!root.isMember("items")) {
    PostMessage("log: ListFiles returned no items...\n");
    return false;
  }

  Json::Value items = root["items"];
  if (!items.isValidIndex(0)) {
    PostMessage("log: Expected items[0] to be valid.\n");
    return false;
  }

  *metadata = items[0U];
  return true;
}

bool Instance::ThreadCreateFile(const char* title,
                                const char* description,
                                const char* content,
                                Json::Value* metadata) {
  InsertFileParams p;
  p.content = content;
  p.description = description;
  p.mime_type = "text/plain";
  p.title = title;

  int32_t result = InsertFile(this, auth_token_, p, metadata);
  if (result != PP_OK) {
    PostMessagef("log: Creating file failed with result %d\n", result);
    return false;
  }

  return true;
}

bool Instance::ThreadUpdateFile(const std::string& file_id,
                                const std::string& content,
                                Json::Value* metadata) {
  InsertFileParams p;
  p.file_id = file_id;
  p.content = content;
  p.mime_type = "text/plain";

  int32_t result = InsertFile(this, auth_token_, p, metadata);
  if (result != PP_OK) {
    PostMessagef("log: Updating file failed with result %d\n", result);
    return false;
  }

  return true;
}

bool Instance::ThreadDownloadFile(const Json::Value& metadata,
                                  std::string* output) {
  ReadUrlParams p;
  p.method = "GET";

  if (!GetMetadataKey(metadata, "downloadUrl", &p.url)) {
    return false;
  }

  AddAuthTokenHeader(&p.request_headers, auth_token_);

  int32_t result = ReadUrl(this, p, output);
  if (result != PP_OK) {
    PostMessagef("log: Downloading failed with result %d\n", result);
    return false;
  }

  return true;
}

bool Instance::GetMetadataKey(const Json::Value& metadata,
                              const char* key,
                              std::string* output) {
  Json::Value value = metadata[key];
  if (!value.isString()) {
    PostMessagef("log: Expected metadata.%s to be a string.\n", key);
    return false;
  }

  *output = value.asString();
  return true;
}

class Module : public pp::Module {
 public:
  Module() : pp::Module() {}
  virtual ~Module() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new Instance(instance);
  }
};

namespace pp {

Module* CreateModule() { return new ::Module(); }

}  // namespace pp
