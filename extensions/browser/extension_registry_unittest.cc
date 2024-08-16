// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

typedef testing::Test ExtensionRegistryTest;

testing::AssertionResult HasSingleExtension(
    const ExtensionList& list,
    const scoped_refptr<const Extension>& extension) {
  if (list.empty()) {
    return testing::AssertionFailure() << "No extensions in list";
  }
  if (list.size() > 1) {
    return testing::AssertionFailure() << list.size()
                                       << " extensions, expected 1";
  }
  const Extension* did_load = list[0].get();
  if (did_load != extension.get()) {
    return testing::AssertionFailure() << "Expected " << extension->id()
                                       << " found " << did_load->id();
  }
  return testing::AssertionSuccess();
}

class TestObserver : public ExtensionRegistryObserver {
 public:
  TestObserver() {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  void Reset() {
    loaded_.clear();
    unloaded_.clear();
    installed_.clear();
    uninstalled_.clear();
  }

  const ExtensionList& loaded() { return loaded_; }
  const ExtensionList& unloaded() { return unloaded_; }
  const ExtensionList& installed() { return installed_; }
  const ExtensionList& uninstalled() { return uninstalled_; }

 private:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override {
    loaded_.push_back(extension);
  }

  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override {
    unloaded_.push_back(extension);
  }

  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override {
    installed_.push_back(extension);
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              UninstallReason reason) override {
    uninstalled_.push_back(extension);
  }

  void OnShutdown(ExtensionRegistry* registry) override { Reset(); }

  ExtensionList loaded_;
  ExtensionList unloaded_;
  ExtensionList installed_;
  ExtensionList uninstalled_;
};

TEST_F(ExtensionRegistryTest, FillAndClearRegistry) {
  ExtensionRegistry registry(nullptr);
  scoped_refptr<const Extension> extension1 = ExtensionBuilder("one").Build();
  scoped_refptr<const Extension> extension2 = ExtensionBuilder("two").Build();
  scoped_refptr<const Extension> extension3 = ExtensionBuilder("three").Build();
  scoped_refptr<const Extension> extension4 = ExtensionBuilder("four").Build();

  // All the sets start empty.
  EXPECT_EQ(0u, registry.enabled_extensions().size());
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blocklisted_extensions().size());

  // Extensions can be added to each set.
  registry.AddEnabled(extension1);
  registry.AddDisabled(extension2);
  registry.AddTerminated(extension3);
  registry.AddBlocklisted(extension4);

  EXPECT_EQ(1u, registry.enabled_extensions().size());
  EXPECT_EQ(1u, registry.disabled_extensions().size());
  EXPECT_EQ(1u, registry.terminated_extensions().size());
  EXPECT_EQ(1u, registry.blocklisted_extensions().size());

  // Clearing the registry clears all sets.
  registry.ClearAll();

  EXPECT_EQ(0u, registry.enabled_extensions().size());
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blocklisted_extensions().size());
}

// A simple test of adding and removing things from sets.
TEST_F(ExtensionRegistryTest, AddAndRemoveExtensionFromRegistry) {
  ExtensionRegistry registry(nullptr);

  // Adding an extension works.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  EXPECT_TRUE(registry.AddEnabled(extension));
  EXPECT_EQ(1u, registry.enabled_extensions().size());

  // The extension was only added to one set.
  EXPECT_EQ(0u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blocklisted_extensions().size());

  // Removing an extension works.
  EXPECT_TRUE(registry.RemoveEnabled(extension->id()));
  EXPECT_EQ(0u, registry.enabled_extensions().size());

  // Trying to remove an extension that isn't in the set fails cleanly.
  EXPECT_FALSE(registry.RemoveEnabled(extension->id()));
}

TEST_F(ExtensionRegistryTest, AddExtensionToRegistryTwice) {
  ExtensionRegistry registry(nullptr);
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();

  // An extension can exist in two sets at once. It would be nice to eliminate
  // this functionality, but some users of ExtensionRegistry need it.
  EXPECT_TRUE(registry.AddEnabled(extension));
  EXPECT_TRUE(registry.AddDisabled(extension));

  EXPECT_EQ(1u, registry.enabled_extensions().size());
  EXPECT_EQ(1u, registry.disabled_extensions().size());
  EXPECT_EQ(0u, registry.terminated_extensions().size());
  EXPECT_EQ(0u, registry.blocklisted_extensions().size());
}

TEST_F(ExtensionRegistryTest, GetExtensionById) {
  ExtensionRegistry registry(nullptr);

  // Trying to get an extension fails cleanly when the sets are empty.
  EXPECT_FALSE(
      registry.GetExtensionById("id", ExtensionRegistry::EVERYTHING));

  scoped_refptr<const Extension> enabled = ExtensionBuilder("enabled").Build();
  scoped_refptr<const Extension> disabled =
      ExtensionBuilder("disabled").Build();
  scoped_refptr<const Extension> terminated =
      ExtensionBuilder("terminated").Build();
  scoped_refptr<const Extension> blocklisted =
      ExtensionBuilder("blocklisted").Build();

  // Add an extension to each set.
  registry.AddEnabled(enabled);
  registry.AddDisabled(disabled);
  registry.AddTerminated(terminated);
  registry.AddBlocklisted(blocklisted);

  // Enabled is part of everything and the enabled list.
  EXPECT_TRUE(
      registry.GetExtensionById(enabled->id(), ExtensionRegistry::EVERYTHING));
  EXPECT_TRUE(registry.enabled_extensions().GetByID(enabled->id()));
  EXPECT_FALSE(registry.disabled_extensions().GetByID(enabled->id()));
  EXPECT_FALSE(registry.terminated_extensions().GetByID(enabled->id()));
  EXPECT_FALSE(registry.blocklisted_extensions().GetByID(enabled->id()));

  // Disabled is part of everything and the disabled list.
  EXPECT_TRUE(
      registry.GetExtensionById(disabled->id(), ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(registry.enabled_extensions().GetByID(disabled->id()));
  EXPECT_TRUE(registry.disabled_extensions().GetByID(disabled->id()));
  EXPECT_FALSE(registry.terminated_extensions().GetByID(disabled->id()));
  EXPECT_FALSE(registry.blocklisted_extensions().GetByID(disabled->id()));

  // Terminated is part of everything and the terminated list.
  EXPECT_TRUE(registry.GetExtensionById(terminated->id(),
                                        ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(registry.enabled_extensions().GetByID(terminated->id()));
  EXPECT_FALSE(registry.disabled_extensions().GetByID(terminated->id()));
  EXPECT_TRUE(registry.terminated_extensions().GetByID(terminated->id()));
  EXPECT_FALSE(registry.blocklisted_extensions().GetByID(terminated->id()));

  // Blocklisted is part of everything and the blocklisted list.
  EXPECT_TRUE(registry.GetExtensionById(blocklisted->id(),
                                        ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(registry.enabled_extensions().GetByID(blocklisted->id()));
  EXPECT_FALSE(registry.disabled_extensions().GetByID(blocklisted->id()));
  EXPECT_FALSE(registry.terminated_extensions().GetByID(blocklisted->id()));
  EXPECT_TRUE(registry.blocklisted_extensions().GetByID(blocklisted->id()));

  // Enabled can be found with multiple flags set.
  EXPECT_TRUE(registry.GetExtensionById(
      enabled->id(),
      ExtensionRegistry::ENABLED | ExtensionRegistry::TERMINATED));

  // Enabled isn't found if the wrong flags are set.
  EXPECT_FALSE(registry.GetExtensionById(
      enabled->id(),
      ExtensionRegistry::DISABLED | ExtensionRegistry::BLOCKLISTED));
}

TEST_F(ExtensionRegistryTest, Observer) {
  ExtensionRegistry registry(nullptr);
  TestObserver observer;
  registry.AddObserver(&observer);

  EXPECT_TRUE(observer.loaded().empty());
  EXPECT_TRUE(observer.unloaded().empty());
  EXPECT_TRUE(observer.installed().empty());

  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();

  registry.TriggerOnWillBeInstalled(extension.get(), false, std::string());
  EXPECT_TRUE(HasSingleExtension(observer.installed(), extension.get()));

  registry.AddEnabled(extension);
  registry.TriggerOnLoaded(extension.get());

  registry.TriggerOnWillBeInstalled(extension.get(), true, "foo");

  EXPECT_TRUE(HasSingleExtension(observer.loaded(), extension.get()));
  EXPECT_TRUE(observer.unloaded().empty());
  registry.Shutdown();

  registry.RemoveEnabled(extension->id());
  registry.TriggerOnUnloaded(extension.get(), UnloadedExtensionReason::DISABLE);

  EXPECT_TRUE(observer.loaded().empty());
  EXPECT_TRUE(HasSingleExtension(observer.unloaded(), extension.get()));
  registry.Shutdown();

  registry.TriggerOnUninstalled(extension.get(), UNINSTALL_REASON_FOR_TESTING);
  EXPECT_TRUE(observer.installed().empty());
  EXPECT_TRUE(HasSingleExtension(observer.uninstalled(), extension.get()));

  registry.RemoveObserver(&observer);
}

// Regression test for https://crbug.com/724563.
TEST_F(ExtensionRegistryTest, TerminatedExtensionStoredVersion) {
  const std::string kVersionString = "1.2.3.4";
  ExtensionRegistry registry(nullptr);
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(base::Value::Dict()
                           .Set("name", "Test")
                           .Set("version", kVersionString)
                           .Set("manifest_version", 2))
          .Build();
  const ExtensionId extension_id = extension->id();

  EXPECT_TRUE(registry.AddEnabled(extension));
  EXPECT_FALSE(registry.terminated_extensions().GetByID(extension_id));
  {
    base::Version version = registry.GetStoredVersion(extension_id);
    ASSERT_TRUE(version.IsValid());
    EXPECT_EQ(kVersionString,
              registry.GetStoredVersion(extension_id).GetString());
  }

  // Simulate terminating |extension|.
  EXPECT_TRUE(registry.RemoveEnabled(extension_id));
  EXPECT_TRUE(registry.AddTerminated(extension));
  EXPECT_TRUE(registry.terminated_extensions().GetByID(extension_id));
  {
    base::Version version = registry.GetStoredVersion(extension_id);
    ASSERT_TRUE(version.IsValid());
    EXPECT_EQ(kVersionString,
              registry.GetStoredVersion(extension_id).GetString());
  }
}

}  // namespace
}  // namespace extensions
