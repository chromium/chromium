// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_ctrl_response_buffer.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/base/parse_number.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_values.h"

namespace net {

// static
const int FtpCtrlResponse::kInvalidStatusCode = -1;

FtpCtrlResponse::FtpCtrlResponse() : status_code(kInvalidStatusCode) {}

FtpCtrlResponse::FtpCtrlResponse(const FtpCtrlResponse& other) = default;

FtpCtrlResponse::~FtpCtrlResponse() = default;

FtpCtrlResponseBuffer::FtpCtrlResponseBuffer(const NetLogWithSource& net_log)
    : multiline_(false), net_log_(net_log) {}

FtpCtrlResponseBuffer::~FtpCtrlResponseBuffer() = default;

int FtpCtrlResponseBuffer::ConsumeData(const char* data, int data_length) {
  buffer_.append(data, data_length);
  ExtractFullLinesFromBuffer();

  while (!lines_.empty()) {
    ParsedLine line = lines_.front();
    lines_.pop();

    if (multiline_) {
      if (!line.is_complete || line.status_code != response_buf_.status_code) {
        line_buf_.append(line.raw_text);
        continue;
      }

      response_buf_.lines.push_back(line_buf_);

      line_buf_ = line.status_text;
      DCHECK_EQ(line.status_code, response_buf_.status_code);

      if (!line.is_multiline) {
        response_buf_.lines.push_back(line_buf_);
        responses_.push(response_buf_);

        // Prepare to handle following lines.
        response_buf_ = FtpCtrlResponse();
        line_buf_.clear();
        multiline_ = false;
      }
    } else {
      if (!line.is_complete)
        return ERR_INVALID_RESPONSE;

      response_buf_.status_code = line.status_code;
      if (line.is_multiline) {
        line_buf_ = line.status_text;
        multiline_ = true;
      } else {
        response_buf_.lines.push_back(line.status_text);
        responses_.push(response_buf_);

        // Prepare to handle following lines.
        response_buf_ = FtpCtrlResponse();
        line_buf_.clear();
      }
    }
  }

  return OK;
}

namespace {

base::Value NetLogFtpCtrlResponseParams(const FtpCtrlResponse* response) {
  base::ListValue lines;
  for (const auto& line : response->lines)
    lines.Append(NetLogStringValue(line));

  base::DictionaryValue dict;
  dict.SetInteger("status_code", response->status_code);
  dict.SetKey("lines", std::move(lines));
  return std::move(dict);
}

}  // namespace

FtpCtrlResponse FtpCtrlResponseBuffer::PopResponse() {
  FtpCtrlResponse result = responses_.front();
  responses_.pop();

  net_log_.AddEvent(NetLogEventType::FTP_CONTROL_RESPONSE,
                    [&] { return NetLogFtpCtrlResponseParams(&result); });

  return result;
}

FtpCtrlResponseBuffer::ParsedLine::ParsedLine()
    : has_status_code(false),
      is_multiline(false),
      is_complete(false),
      status_code(FtpCtrlResponse::kInvalidStatusCode) {
}

FtpCtrlResponseBuffer::ParsedLine::ParsedLine(const ParsedLine& other) =
    default;

// static
FtpCtrlResponseBuffer::ParsedLine FtpCtrlResponseBuffer::ParseLine(
    const std::string& line) {
  ParsedLine result;

  if (line.length() >= 3) {
    if (ParseInt32(base::StringPiece(line.begin(), line.begin() + 3),
                   ParseIntFormat::NON_NEGATIVE, &result.status_code)) {
      result.has_status_code =
          (100 <= result.status_code && result.status_code <= 599);
    }
    if (result.has_status_code && line.length() >= 4 && line[3] == ' ') {
      result.is_complete = true;
    } else if (result.has_status_code && line.length() >= 4 && line[3] == '-') {
      result.is_complete = true;
      result.is_multiline = true;
    }
  }

  if (result.is_complete) {
    result.status_text = line.substr(4);
  } else {
    result.status_text = line;
  }

  result.raw_text = line;

  return result;
}

void FtpCtrlResponseBuffer::ExtractFullLinesFromBuffer() {
  int cut_pos = 0;
  for (size_t i = 0; i < buffer_.length(); i++) {
    if (i >= 1 && buffer_[i - 1] == '\r' && buffer_[i] == '\n') {
      lines_.push(ParseLine(buffer_.substr(cut_pos, i - cut_pos - 1)));
      cut_pos = i + 1;
    }
  }
  buffer_.erase(0, cut_pos);
}

}  // namespace net
