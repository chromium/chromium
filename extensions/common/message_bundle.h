// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MESSAGE_BUNDLE_H_
#define EXTENSIONS_COMMON_MESSAGE_BUNDLE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/values.h"

namespace extensions {

// Contains localized extension messages for one locale. Any messages that the
// locale does not provide are pulled from the default locale.
class MessageBundle {
 public:
  using SubstitutionMap = std::map<std::string, std::string>;
  using CatalogVector = std::vector<base::Value::Dict>;

  // JSON keys of interest for messages file.
  static const char kContentKey[];
  static const char kMessageKey[];
  static const char kPlaceholdersKey[];

  // Begin/end markers for placeholders and messages
  static const char kPlaceholderBegin[];
  static const char kPlaceholderEnd[];
  static const char kMessageBegin[];
  static const char kMessageEnd[];

  // Reserved message names in the dictionary.
  // Update i18n documentation when adding new reserved value.
  static const char kUILocaleKey[];
  // See http://code.google.com/apis/gadgets/docs/i18n.html#BIDI for
  // description.
  // TODO(cira): point to chrome docs once they are out.
  static const char kBidiDirectionKey[];
  static const char kBidiReversedDirectionKey[];
  static const char kBidiStartEdgeKey[];
  static const char kBidiEndEdgeKey[];
  // Extension id gets added in the
  // browser/renderer_host/resource_message_filter.cc to enable message
  // replacement for non-localized extensions.
  static const char kExtensionIdKey[];

  // Values for some of the reserved messages.
  static const char kBidiLeftEdgeValue[];
  static const char kBidiRightEdgeValue[];

  // Creates MessageBundle or returns NULL if there was an error. Expects
  // locale_catalogs to be sorted from more specific to less specific, with
  // default catalog at the end.
  static MessageBundle* Create(const CatalogVector& locale_catalogs,
                               std::string* error);

  // Get message from the catalog with given key.
  // Returned message has all of the internal placeholders resolved to their
  // value (content).
  // Returns empty string if it can't find a message.
  // We don't use simple GetMessage name, since there is a global
  // #define GetMessage GetMessageW override in Chrome code.
  std::string GetL10nMessage(const std::string& name) const;

  // Get message from the given catalog with given key.
  static std::string GetL10nMessage(const std::string& name,
                                    const SubstitutionMap& dictionary);

  // Number of messages in the catalog.
  // Used for unittesting only.
  size_t size() const { return dictionary_.size(); }

  // Replaces all __MSG_message__ with values from the catalog.
  // Returns false if there is a message in text that's not defined in the
  // dictionary.
  bool ReplaceMessages(std::string* text, std::string* error) const;
  // Static version that accepts dictionary.
  static bool ReplaceMessagesWithExternalDictionary(
      const SubstitutionMap& dictionary, std::string* text, std::string* error);

  // Replaces each occurance of variable placeholder with its value.
  // I.e. replaces __MSG_name__ with value from the catalog with the key "name".
  // Returns false if for a valid message/placeholder name there is no matching
  // replacement.
  // Public for easier unittesting.
  static bool ReplaceVariables(const SubstitutionMap& variables,
                               const std::string& var_begin,
                               const std::string& var_end,
                               std::string* message,
                               std::string* error);

  // Allow only ascii 0-9, a-z, A-Z, and _ in the variable name.
  // Returns false if the input is empty or if it has illegal characters.
  static bool IsValidName(const std::string& name);

  // Getter for dictionary_.
  const SubstitutionMap* dictionary() const { return &dictionary_; }

  ~MessageBundle();

 private:
  // Testing friend.
  friend class MessageBundleTest;

  // Use Create to create MessageBundle instance.
  MessageBundle();

  // Initializes the instance from the contents of vector of catalogs.
  // If the key is not present in more specific catalog we fall back to next one
  // (less specific).
  // Returns false on error.
  bool Init(const CatalogVector& locale_catalogs, std::string* error);

  // Appends locale specific reserved messages to the dictionary.
  // Returns false if there was a conflict with user defined messages.
  bool AppendReservedMessagesForLocale(const std::string& application_locale,
                                       std::string* error);

  // Helper methods that navigate JSON tree and return simplified message.
  // They replace all $PLACEHOLDERS$ with their value, and return just key/value
  // of the message.
  bool GetMessageValue(const std::string& key,
                       const base::Value& name_value,
                       std::string* value,
                       std::string* error) const;

  // Get all placeholders for a given message from JSON subtree.
  bool GetPlaceholders(const base::Value::Dict& name_tree,
                       const std::string& name_key,
                       SubstitutionMap* placeholders,
                       std::string* error) const;

  // For a given message, replaces all placeholders with their actual value.
  // Returns false if replacement failed (see ReplaceVariables).
  bool ReplacePlaceholders(const SubstitutionMap& placeholders,
                           std::string* message,
                           std::string* error) const;

  // Holds all messages for application locale.
  SubstitutionMap dictionary_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MESSAGE_BUNDLE_H_
