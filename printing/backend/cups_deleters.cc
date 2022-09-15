// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_deleters.h"

namespace printing {

void HttpDeleter::operator()(http_t* http) const {
  httpClose(http);
}

void DestinationDeleter::operator()(cups_dest_t* dest) const {
  cupsFreeDests(1, dest);
}

void DestInfoDeleter::operator()(cups_dinfo_t* info) const {
  cupsFreeDestInfo(info);
}

void OptionDeleter::operator()(cups_option_t* option) const {
  // Frees the name and value buffers then the struct itself
  cupsFreeOptions(1, option);
}

}  // namespace printing
