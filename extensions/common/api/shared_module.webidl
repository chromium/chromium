// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary Import {
  // Extension ID of the shared module this extension or app depends on.
  required DOMString id;

  // Minimum supported version of the shared module.
  DOMString minimum_version;
};

dictionary Export {
  // Optional list of extension IDs explicitly allowed to import this Shared
  // Module's resources.  If no allowlist is given, all extensions are allowed
  // to import it.
  sequence<DOMString> allowlist;
};

// Stub namespace for the "import" and "export" manifest keys.
[generate_error_messages, Namespace=sharedModule]
partial dictionary ExtensionManifest {
  // The import field is used by extensions and apps to declare that they
  // depend on the resources from particular Shared Modules.
  sequence<Import> import;

  // The export field indicates an extension is a Shared Module that exports
  // its resources.
  Export export;
};
