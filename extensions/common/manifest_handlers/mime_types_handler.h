// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "third_party/skia/include/core/SkColor.h"

class MimeTypesHandler {
 public:
  // Returns list of extensions' ids that are allowed to use MIME type filters.
  static std::vector<std::string> GetMIMETypeWhitelist();

  static MimeTypesHandler* GetHandler(const extensions::Extension* extension);

  MimeTypesHandler();
  ~MimeTypesHandler();

  // extension id
  std::string extension_id() const { return extension_id_; }
  void set_extension_id(const std::string& extension_id) {
    extension_id_ = extension_id;
  }

  // Adds a MIME type filter to the handler.
  void AddMIMEType(const std::string& mime_type);
  // Tests if the handler has registered a filter for the MIME type.
  bool CanHandleMIMEType(const std::string& mime_type) const;

  // Set the URL that will be used to handle MIME type requests.
  void set_handler_url(const std::string& handler_url) {
    handler_url_ = handler_url;
  }
  // The URL that will be used to handle MIME type requests.
  const std::string& handler_url() const { return handler_url_; }

  const std::set<std::string>& mime_type_set() const { return mime_type_set_; }

  // Returns true if this MimeTypesHandler has a plugin associated with it (for
  // the mimeHandlerPrivate API).
  bool HasPlugin() const;

  // If HasPlugin() returns true, this will return the plugin path for the
  // plugin associated with this MimeTypesHandler.
  base::FilePath GetPluginPath() const;

  // Returns the background color used by the mime handler.
  SkColor GetBackgroundColor() const;

 private:
  // The id for the extension this action belongs to (as defined in the
  // extension manifest).
  std::string extension_id_;

  // A list of MIME type filters.
  std::set<std::string> mime_type_set_;

  std::string handler_url_;
};

class MimeTypesHandlerParser : public extensions::ManifestHandler {
 public:
  MimeTypesHandlerParser();
  ~MimeTypesHandlerParser() override;

  bool Parse(extensions::Extension* extension, base::string16* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_
