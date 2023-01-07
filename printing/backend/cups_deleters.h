// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_DELETERS_H_
#define PRINTING_BACKEND_CUPS_DELETERS_H_

#include <cups/cups.h>
#include <memory>

#include "base/component_export.h"

namespace printing {

struct COMPONENT_EXPORT(PRINT_BACKEND) HttpDeleter {
  void operator()(http_t* http) const;
};

struct COMPONENT_EXPORT(PRINT_BACKEND) DestinationDeleter {
  void operator()(cups_dest_t* dest) const;
};

struct COMPONENT_EXPORT(PRINT_BACKEND) DestInfoDeleter {
  void operator()(cups_dinfo_t* info) const;
};

struct COMPONENT_EXPORT(PRINT_BACKEND) OptionDeleter {
  void operator()(cups_option_t* option) const;
};

using ScopedHttpPtr = std::unique_ptr<http_t, HttpDeleter>;
using ScopedDestination = std::unique_ptr<cups_dest_t, DestinationDeleter>;
using ScopedDestInfo = std::unique_ptr<cups_dinfo_t, DestInfoDeleter>;
using ScopedCupsOption = std::unique_ptr<cups_option_t, OptionDeleter>;

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_DELETERS_H_
