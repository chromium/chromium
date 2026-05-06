// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handler.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

class MimeTypesHandler {
 public:
  // Per-type configuration. Populated for both dict and legacy formats.
  struct MimeTypeConfig {
    MimeTypeConfig();
    MimeTypeConfig(const MimeTypeConfig&);
    MimeTypeConfig& operator=(const MimeTypeConfig&);
    ~MimeTypeConfig();

    GURL handler_url;
    bool can_embed = false;
  };

  // Returns list of extensions' ids that are allowed to use MIME type filters.
  static const std::vector<extensions::ExtensionId>& GetMIMETypeAllowlist();

  // Returns list of MIME types allowed for public (non-allowlisted) extensions.
  static base::span<const std::string_view> GetPublicAllowedMIMETypeList();

  static const MimeTypesHandler* Get(const extensions::Extension& extension);

  MimeTypesHandler();
  ~MimeTypesHandler();

  extensions::ExtensionId extension_id() const { return extension_id_; }
  void set_extension_id(const extensions::ExtensionId& extension_id) {
    extension_id_ = extension_id;
  }

  // Adds a MIME type with per-type config.
  // `handler_url` must be a fully resolved extension resource URL.
  void AddMIMEType(const std::string& mime_type,
                   const GURL& handler_url,
                   bool can_embed);

  // Returns `true` if this handler's `extension_id` is from the allowed list.
  // These extension are historically treated as plugins.
  bool IsPluginExtension() const;

  // Returns the fully resolved handler URL for `mime_type`.
  // Returns an empty GURL if no per-type config exists.
  GURL GetHandlerUrl(const std::string& mime_type) const;

  // Returns whether `mime_type` supports embedding (iframe/embed/object).
  // CHECKs that plugin (legacy manifest format) handlers never set can_embed.
  bool CanEmbedMimeType(const std::string& mime_type) const;

  // Returns the list of MIME types this handler supports.
  std::vector<std::string> GetSupportedMimeTypes() const;

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
  extensions::ExtensionId extension_id_;

  // Per-type handler config. Populated for both dict and legacy formats.
  base::flat_map<std::string, MimeTypeConfig> per_type_configs_;
};

class MimeTypesHandlerParser : public extensions::ManifestHandler {
 public:
  MimeTypesHandlerParser();
  ~MimeTypesHandlerParser() override;

  bool Parse(extensions::Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_MIME_TYPES_HANDLER_H_
