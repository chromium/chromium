// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_IPP_HANDLERS_H_
#define PRINTING_BACKEND_IPP_HANDLERS_H_

#include "printing/backend/print_backend.h"

namespace printing {

class CupsOptionProvider;

// Ignore attribute (e.g. already implemented in print preview).
void NoOpHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities* capabilities);

// Text-based attribute.
void TextHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities* capabilities);

// Number-based attribute.
void NumberHandler(const CupsOptionProvider& printer,
                   const char* attribute_name,
                   AdvancedCapabilities* capabilities);

// Boolean attribute.
void BooleanHandler(const CupsOptionProvider& printer,
                    const char* attribute_name,
                    AdvancedCapabilities* capabilities);

// Attribute with a list of supported text values.
void KeywordHandler(const CupsOptionProvider& printer,
                    const char* attribute_name,
                    AdvancedCapabilities* capabilities);

// Attribute with a list of supported integer values.
void EnumHandler(const CupsOptionProvider& printer,
                 const char* attribute_name,
                 AdvancedCapabilities* capabilities);

// Attribute that takes subsets of supported integer values.
// `none_value` is ignored: it's used since IPP doesn't allow empty sets here.
void MultivalueEnumHandler(int none_value,
                           const CupsOptionProvider& printer,
                           const char* attribute_name,
                           AdvancedCapabilities* capabilities);

}  // namespace printing

#endif  // PRINTING_BACKEND_IPP_HANDLERS_H_
