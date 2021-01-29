// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handler.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/scoped_testing_manifest_handler_registry.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

std::vector<std::string> SingleKey(const std::string& key) {
  return std::vector<std::string>(1, key);
}

}  // namespace

class ManifestHandlerTest : public testing::Test {
 public:
  class ParsingWatcher {
   public:
    // Called when a manifest handler parses.
    void Record(const std::string& name) {
      parsed_names_.push_back(name);
    }

    const std::vector<std::string>& parsed_names() {
      return parsed_names_;
    }

    // Returns true if |name_before| was parsed before |name_after|.
    bool ParsedBefore(const std::string& name_before,
                      const std::string& name_after) {
      size_t i_before = parsed_names_.size();
      size_t i_after = 0;
      for (size_t i = 0; i < parsed_names_.size(); ++i) {
        if (parsed_names_[i] == name_before)
          i_before = i;
        if (parsed_names_[i] == name_after)
          i_after = i;
      }
      if (i_before < i_after)
        return true;
      return false;
    }

   private:
    // The order of manifest handlers that we watched parsing.
    std::vector<std::string> parsed_names_;
  };

  class TestManifestHandler : public ManifestHandler {
   public:
    TestManifestHandler(const std::string& name,
                        const std::vector<std::string>& keys,
                        const std::vector<std::string>& prereqs,
                        ParsingWatcher* watcher)
        : name_(name), keys_(keys), prereqs_(prereqs), watcher_(watcher) {
      keys_ptrs_.resize(keys_.size());
      std::transform(keys_.begin(), keys_.end(), keys_ptrs_.begin(),
                     [](const std::string& s) { return s.c_str(); });
    }

    bool Parse(Extension* extension, base::string16* error) override {
      watcher_->Record(name_);
      return true;
    }

    const std::vector<std::string> PrerequisiteKeys() const override {
      return prereqs_;
    }

   protected:
    std::string name_;
    std::vector<std::string> keys_;
    std::vector<const char*> keys_ptrs_;
    std::vector<std::string> prereqs_;
    ParsingWatcher* watcher_;

    base::span<const char* const> Keys() const override { return keys_ptrs_; }
  };

  class FailingTestManifestHandler : public TestManifestHandler {
   public:
    FailingTestManifestHandler(const std::string& name,
                               const std::vector<std::string>& keys,
                               const std::vector<std::string>& prereqs,
                               ParsingWatcher* watcher)
        : TestManifestHandler(name, keys, prereqs, watcher) {
    }
    bool Parse(Extension* extension, base::string16* error) override {
      *error = base::ASCIIToUTF16(name_);
      return false;
    }
  };

  class AlwaysParseTestManifestHandler : public TestManifestHandler {
   public:
    AlwaysParseTestManifestHandler(const std::string& name,
                                   const std::vector<std::string>& keys,
                                   const std::vector<std::string>& prereqs,
                                   ParsingWatcher* watcher)
        : TestManifestHandler(name, keys, prereqs, watcher) {
    }

    bool AlwaysParseForType(Manifest::Type type) const override { return true; }
  };

  class TestManifestValidator : public ManifestHandler {
   public:
    TestManifestValidator(bool return_value,
                          bool always_validate,
                          std::vector<std::string> keys)
        : return_value_(return_value),
          always_validate_(always_validate),
          keys_(keys) {
      keys_ptrs_.resize(keys_.size());
      std::transform(keys_.begin(), keys_.end(), keys_ptrs_.begin(),
                     [](const std::string& s) { return s.c_str(); });
    }

    bool Parse(Extension* extension, base::string16* error) override {
      return true;
    }

    bool Validate(const Extension* extension,
                  std::string* error,
                  std::vector<InstallWarning>* warnings) const override {
      return return_value_;
    }

    bool AlwaysValidateForType(Manifest::Type type) const override {
      return always_validate_;
    }

   private:
    base::span<const char* const> Keys() const override { return keys_ptrs_; }

   protected:
    bool return_value_;
    bool always_validate_;
    std::vector<std::string> keys_;
    std::vector<const char*> keys_ptrs_;
  };
};

TEST_F(ManifestHandlerTest, DependentHandlers) {
  ScopedTestingManifestHandlerRegistry scoped_registry;
  ParsingWatcher watcher;
  std::vector<std::string> prereqs;
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();
  registry->RegisterHandler(std::make_unique<TestManifestHandler>(
      "A", SingleKey("a"), prereqs, &watcher));
  registry->RegisterHandler(std::make_unique<TestManifestHandler>(
      "B", SingleKey("b"), prereqs, &watcher));
  registry->RegisterHandler(std::make_unique<TestManifestHandler>(
      "J", SingleKey("j"), prereqs, &watcher));
  registry->RegisterHandler(std::make_unique<AlwaysParseTestManifestHandler>(
      "K", SingleKey("k"), prereqs, &watcher));
  prereqs.push_back("c.d");
  std::vector<std::string> keys;
  keys.push_back("c.e");
  keys.push_back("c.z");
  registry->RegisterHandler(
      std::make_unique<TestManifestHandler>("C.EZ", keys, prereqs, &watcher));
  prereqs.clear();
  prereqs.push_back("b");
  prereqs.push_back("k");
  registry->RegisterHandler(std::make_unique<TestManifestHandler>(
      "C.D", SingleKey("c.d"), prereqs, &watcher));
  ManifestHandler::FinalizeRegistration();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "no name")
                           .Set("version", "0")
                           .Set("manifest_version", 2)
                           .Set("a", 1)
                           .Set("b", 2)
                           .Set("c", DictionaryBuilder()
                                         .Set("d", 3)
                                         .Set("e", 4)
                                         .Set("f", 5)
                                         .Build())
                           .Set("g", 6)
                           .Build())
          .Build();

  // A, B, C.EZ, C.D, K
  EXPECT_EQ(5u, watcher.parsed_names().size());
  EXPECT_TRUE(watcher.ParsedBefore("B", "C.D"));
  EXPECT_TRUE(watcher.ParsedBefore("K", "C.D"));
  EXPECT_TRUE(watcher.ParsedBefore("C.D", "C.EZ"));
}

TEST_F(ManifestHandlerTest, FailingHandlers) {
  ScopedTestingManifestHandlerRegistry scoped_registry;
  // Can't use ExtensionBuilder, because this extension will fail to
  // be parsed.
  std::unique_ptr<base::DictionaryValue> manifest_a(
      DictionaryBuilder()
          .Set("name", "no name")
          .Set("version", "0")
          .Set("manifest_version", 2)
          .Set("a", 1)
          .Build());

  // Succeeds when "a" is not recognized.
  std::string error;
  scoped_refptr<Extension> extension =
      Extension::Create(base::FilePath(), Manifest::INVALID_LOCATION,
                        *manifest_a, Extension::NO_FLAGS, &error);
  EXPECT_TRUE(extension.get());

  // Register a handler for "a" that fails.
  ParsingWatcher watcher;
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();
  registry->RegisterHandler(std::make_unique<FailingTestManifestHandler>(
      "A", SingleKey("a"), std::vector<std::string>(), &watcher));
  ManifestHandler::FinalizeRegistration();

  extension = Extension::Create(
      base::FilePath(),
      Manifest::INVALID_LOCATION,
      *manifest_a,
      Extension::NO_FLAGS,
      &error);
  EXPECT_FALSE(extension.get());
  EXPECT_EQ("A", error);
}

TEST_F(ManifestHandlerTest, Validate) {
  ScopedTestingManifestHandlerRegistry scoped_registry;
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                           .Set("name", "no name")
                           .Set("version", "0")
                           .Set("manifest_version", 2)
                           .Set("a", 1)
                           .Set("b", 2)
                           .Build())
          .Build();
  EXPECT_TRUE(extension.get());

  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();
  std::string error;
  std::vector<InstallWarning> warnings;
  // Always validates and fails.
  registry->RegisterHandler(
      std::make_unique<TestManifestValidator>(false, true, SingleKey("c")));
  EXPECT_FALSE(
      ManifestHandler::ValidateExtension(extension.get(), &error, &warnings));

  // Validates "a" and fails.
  registry->RegisterHandler(
      std::make_unique<TestManifestValidator>(false, true, SingleKey("a")));
  EXPECT_FALSE(
      ManifestHandler::ValidateExtension(extension.get(), &error, &warnings));
}

}  // namespace extensions
