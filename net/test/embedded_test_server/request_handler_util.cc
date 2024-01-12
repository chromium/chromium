// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/embedded_test_server/request_handler_util.h"

#include <stdlib.h>

#include <ctime>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/base/url_util.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "url/gurl.h"

namespace net::test_server {
constexpr base::FilePath::CharType kMockHttpHeadersExtension[] =
    FILE_PATH_LITERAL("mock-http-headers");

std::string GetContentType(const base::FilePath& path) {
  if (path.MatchesExtension(FILE_PATH_LITERAL(".crx")))
    return "application/x-chrome-extension";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".css")))
    return "text/css";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".exe")))
    return "application/octet-stream";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".gif")))
    return "image/gif";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".gzip")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".gz"))) {
    return "application/x-gzip";
  }
  if (path.MatchesExtension(FILE_PATH_LITERAL(".jpeg")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".jpg"))) {
    return "image/jpeg";
  }
  if (path.MatchesExtension(FILE_PATH_LITERAL(".js")))
    return "application/javascript";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".json")))
    return "application/json";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".pdf")))
    return "application/pdf";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".svg")))
    return "image/svg+xml";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".txt")))
    return "text/plain";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".wav")))
    return "audio/wav";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".webp")))
    return "image/webp";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".mp4")))
    return "video/mp4";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".webm")))
    return "video/webm";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".xml")))
    return "text/xml";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".mhtml")))
    return "multipart/related";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".mht")))
    return "message/rfc822";
  if (path.MatchesExtension(FILE_PATH_LITERAL(".html")) ||
      path.MatchesExtension(FILE_PATH_LITERAL(".htm"))) {
    return "text/html";
  }
  return "";
}

bool ShouldHandle(const HttpRequest& request, const std::string& path_prefix) {
  if (request.method == METHOD_CONNECT) {
    return false;
  }

  GURL url = request.GetURL();
  return url.path() == path_prefix || url.path().starts_with(path_prefix + "/");
}

std::unique_ptr<HttpResponse> HandlePrefixedRequest(
    const std::string& prefix,
    const EmbeddedTestServer::HandleRequestCallback& handler,
    const HttpRequest& request) {
  if (ShouldHandle(request, prefix))
    return handler.Run(request);
  return nullptr;
}

RequestQuery ParseQuery(const GURL& url) {
  RequestQuery queries;
  for (QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    std::string unescaped_query = base::UnescapeBinaryURLComponent(
        it.GetKey(), base::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
    queries[unescaped_query].push_back(it.GetUnescapedValue());
  }
  return queries;
}

std::string GetFilePathWithReplacements(
    const std::string& original_file_path,
    const base::StringPairs& text_to_replace) {
  std::string new_file_path = original_file_path;
  for (const auto& replacement : text_to_replace) {
    const std::string& old_text = replacement.first;
    const std::string& new_text = replacement.second;
    std::string base64_old = base::Base64Encode(old_text);
    std::string base64_new = base::Base64Encode(new_text);
    if (new_file_path == original_file_path)
      new_file_path += "?";
    else
      new_file_path += "&";
    new_file_path += "replace_text=";
    new_file_path += base64_old;
    new_file_path += ":";
    new_file_path += base64_new;
  }

  return new_file_path;
}

// Returns false if there were errors, otherwise true.
bool UpdateReplacedText(const RequestQuery& query, std::string* data) {
  auto replace_text = query.find("replace_text");
  if (replace_text == query.end())
    return true;

  for (const auto& replacement : replace_text->second) {
    if (replacement.find(":") == std::string::npos)
      return false;
    std::string find;
    std::string with;
    base::Base64Decode(replacement.substr(0, replacement.find(":")), &find);
    base::Base64Decode(replacement.substr(replacement.find(":") + 1), &with);
    base::ReplaceSubstringsAfterOffset(data, 0, find, with);
  }

  return true;
}

// Handles |request| by serving a file from under |server_root|.
std::unique_ptr<HttpResponse> HandleFileRequest(
    const base::FilePath& server_root,
    const HttpRequest& request) {
  // This is a test-only server. Ignore I/O thread restrictions.
  // TODO(svaldez): Figure out why thread is I/O restricted in the first place.
  base::ScopedAllowBlockingForTesting allow_blocking;

  if (request.method == METHOD_CONNECT) {
    return nullptr;
  }

  // A proxy request will have an absolute path. Simulate the proxy by stripping
  // the scheme, host, and port.
  GURL request_url = request.GetURL();
  std::string relative_path(request_url.path());

  std::string_view post_prefix("/post/");
  if (relative_path.starts_with(post_prefix)) {
    if (request.method != METHOD_POST)
      return nullptr;
    relative_path = relative_path.substr(post_prefix.size() - 1);
  }

  RequestQuery query = ParseQuery(request_url);

  auto failed_response = std::make_unique<BasicHttpResponse>();
  failed_response->set_code(HTTP_NOT_FOUND);

  if (query.find("expected_body") != query.end()) {
    if (request.content.find(query["expected_body"].front()) ==
        std::string::npos) {
      return failed_response;
    }
  }

  if (query.find("expected_headers") != query.end()) {
    for (const auto& header : query["expected_headers"]) {
      if (header.find(":") == std::string::npos)
        return failed_response;
      std::string key = header.substr(0, header.find(":"));
      std::string value = header.substr(header.find(":") + 1);
      if (request.headers.find(key) == request.headers.end() ||
          request.headers.at(key) != value) {
        return failed_response;
      }
    }
  }

  // Trim the first byte ('/').
  DCHECK(relative_path.starts_with("/"));
  std::string request_path = relative_path.substr(1);
  base::FilePath file_path(server_root.AppendASCII(request_path));
  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    file_path = file_path.AppendASCII("index.html");
    if (!base::ReadFileToString(file_path, &file_contents))
      return nullptr;
  }

  if (request.method == METHOD_HEAD)
    file_contents = "";

  if (!UpdateReplacedText(query, &file_contents))
    return failed_response;

  base::FilePath headers_path(
      file_path.AddExtension(kMockHttpHeadersExtension));

  if (base::PathExists(headers_path)) {
    std::string headers_contents;

    if (!base::ReadFileToString(headers_path, &headers_contents) ||
        !UpdateReplacedText(query, &headers_contents)) {
      return nullptr;
    }

    return std::make_unique<RawHttpResponse>(headers_contents, file_contents);
  }

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(HTTP_OK);

  if (request.headers.find("Range") != request.headers.end()) {
    std::vector<HttpByteRange> ranges;

    if (HttpUtil::ParseRangeHeader(request.headers.at("Range"), &ranges) &&
        ranges.size() == 1) {
      ranges[0].ComputeBounds(file_contents.size());
      size_t start = ranges[0].first_byte_position();
      size_t end = ranges[0].last_byte_position();

      http_response->set_code(HTTP_PARTIAL_CONTENT);
      http_response->AddCustomHeader(
          "Content-Range",
          base::StringPrintf("bytes %" PRIuS "-%" PRIuS "/%" PRIuS, start, end,
                             file_contents.size()));

      file_contents = file_contents.substr(start, end - start + 1);
    }
  }

  http_response->set_content_type(GetContentType(file_path));
  http_response->AddCustomHeader("Accept-Ranges", "bytes");
  http_response->AddCustomHeader("ETag", "'" + file_path.MaybeAsASCII() + "'");
  http_response->set_content(file_contents);
  return http_response;
}

}  // namespace net::test_server
