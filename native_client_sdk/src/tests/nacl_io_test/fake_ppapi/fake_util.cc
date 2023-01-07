// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_util.h"

#include <strings.h>

#include <string>

#include <ppapi/c/pp_completion_callback.h>
#include <ppapi/c/pp_errors.h>

// Helper function to call the completion callback if it is defined (an
// asynchronous call), or return the result directly if it isn't (a synchronous
// call).
//
// Use like this:
//   if (<some error condition>)
//     return RunCompletionCallback(callback, PP_ERROR_FUBAR);
//
//   /* Everything worked OK */
//   return RunCompletionCallback(callback, PP_OK);
int32_t RunCompletionCallback(PP_CompletionCallback* callback, int32_t result) {
  if (callback->func) {
    PP_RunCompletionCallback(callback, result);
    return PP_OK_COMPLETIONPENDING;
  }
  return result;
}

void SetHeader(const std::string& key,
               const std::string& value,
               std::string* out_headers) {
  std::string old_value;
  if (!GetHeaderValue(*out_headers, key, &old_value)) {
    *out_headers += key + ": " + value + "\n";
    return;
  }

  size_t offset = 0;
  while (offset != std::string::npos) {
    // Find the next colon; this separates the key from the value.
    size_t colon = out_headers->find(':', offset);

    // Find the newline; this separates the value from the next header.
    size_t newline = out_headers->find('\n', offset);
    if (strncasecmp(key.c_str(), &out_headers->data()[offset], key.size()) ==
        0) {
      *out_headers = out_headers->substr(0, colon) + ": " + value +
                     out_headers->substr(newline);

      return;
    }

    // Key doesn't match, skip to next header.
    offset = newline + 1;
  }
}

bool GetHeaderValue(const std::string& headers,
                    const std::string& key,
                    std::string* out_value) {
  out_value->clear();

  size_t offset = 0;
  while (offset != std::string::npos) {
    // Find the next colon; this separates the key from the value.
    size_t colon = headers.find(':', offset);
    if (colon == std::string::npos)
      return false;

    // Find the newline; this separates the value from the next header.
    size_t newline = headers.find('\n', offset);
    if (strncasecmp(key.c_str(), &headers.data()[offset], key.size()) != 0) {
      // Key doesn't match, skip to next header.
      offset = newline + 1;
      continue;
    }

    // Key matches, extract value. First, skip leading spaces.
    size_t nonspace = headers.find_first_not_of(' ', colon + 1);
    if (nonspace == std::string::npos)
      return false;

    out_value->assign(headers, nonspace, newline - nonspace);
    return true;
  }

  return false;
}
