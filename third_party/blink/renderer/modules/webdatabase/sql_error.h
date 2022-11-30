/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_ERROR_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class SQLErrorData {
  USING_FAST_MALLOC(SQLErrorData);

 public:
  static std::unique_ptr<SQLErrorData> Create(unsigned code,
                                              const char* message,
                                              int sqlite_code,
                                              const char* sqlite_message) {
    return std::make_unique<SQLErrorData>(
        code,
        String::Format("%s (%d %s)", message, sqlite_code, sqlite_message));
  }

  SQLErrorData(unsigned code, const String& message)
      : code_(code), message_(message) {}

  unsigned Code() const { return code_; }
  String Message() const { return message_; }

 private:
  unsigned code_;
  String message_;
};

class SQLError final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SQLError(const SQLErrorData& data) : data_(data) {}

  unsigned code() const { return data_.Code(); }
  String message() const { return data_.Message(); }

  enum SQLErrorCode {
    kUnknownErr = 0,
    kDatabaseErr = 1,
    kVersionErr = 2,
    kTooLargeErr = 3,
    kQuotaErr = 4,
    kSyntaxErr = 5,
    kConstraintErr = 6,
    kTimeoutErr = 7
  };

  static const char kQuotaExceededErrorMessage[];
  static const char kUnknownErrorMessage[];
  static const char kVersionErrorMessage[];

 private:
  const SQLErrorData data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_ERROR_H_
