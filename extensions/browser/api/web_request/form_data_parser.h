// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_FORM_DATA_PARSER_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_FORM_DATA_PARSER_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/values.h"

namespace net {
class HttpRequestHeaders;
}

namespace extensions {

// Interface for the form data parsers.
class FormDataParser {
 public:
  // Result encapsulates name-value pairs returned by GetNextNameValue.
  // Value stored as base::Value, which is string if data is UTF-8 string and
  // binary blob if value represents form data binary data.
  class Result {
   public:
    Result();

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    ~Result();

    const std::string& name() const { return name_; }
    base::Value take_value() { return std::move(value_); }

    void set_name(std::string_view str) { name_ = str; }
    void SetBinaryValue(std::string_view str);
    void SetStringValue(std::string str);

   private:
    std::string name_;
    base::Value value_;
  };

  FormDataParser(const FormDataParser&) = delete;
  FormDataParser& operator=(const FormDataParser&) = delete;

  virtual ~FormDataParser();

  // Creates a correct parser instance based on the |request_headers|. Returns
  // null on failure.
  static std::unique_ptr<FormDataParser> Create(
      const net::HttpRequestHeaders& request_headers);

  // Creates a correct parser instance based on |content_type_header|, the
  // "Content-Type" request header value. If |content_type_header| is NULL, it
  // defaults to "application/x-www-form-urlencoded". Returns NULL on failure.
  static std::unique_ptr<FormDataParser> CreateFromContentTypeHeader(
      const std::string* content_type_header);

  // Returns true if there was some data, it was well formed and all was read.
  virtual bool AllDataReadOK() = 0;

  // Gets the next name-value pair from the source data and stores it in
  // |result|. Returns true if a pair was found. Callers must have previously
  // succesfully called the SetSource method.
  virtual bool GetNextNameValue(Result* result) = 0;

  // Sets the |source| of the data to be parsed. One form data parser is only
  // expected to be associated with one source, so generally, SetSource should
  // be only called once. However, for technical reasons, the source might only
  // be available in chunks for multipart encoded forms, in which case it is OK
  // to call SetSource multiple times to add all chunks of a single source. The
  // ownership of |source| is left with the caller and the source should live
  // until |this| dies or |this->SetSource()| is called again, whichever comes
  // sooner. Returns true on success.
  virtual bool SetSource(std::string_view source) = 0;

 protected:
  FormDataParser();
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_FORM_DATA_PARSER_H_
