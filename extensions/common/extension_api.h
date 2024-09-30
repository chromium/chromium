// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EXTENSION_API_H_
#define EXTENSIONS_COMMON_EXTENSION_API_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "extensions/common/context_data.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"
#include "extensions/common/url_pattern_set.h"

class GURL;

namespace extensions {

class Extension;
class ExtensionsClient;
class Feature;

// Used when testing Feature availability to specify whether feature aliases
// should be ignored or not - i.e. if a feature exposed only through an alias
// should be considered available.
enum class CheckAliasStatus {
  // Includes aliases in an availability check.
  ALLOWED,
  // Ignores aliases during an availability check.
  NOT_ALLOWED
};

// C++ Wrapper for the JSON API definitions in chrome/common/extensions/api/.
//
// WARNING: This class is accessed on multiple threads in the browser process
// (see ExtensionFunctionDispatcher). No state should be modified after
// construction.
class ExtensionAPI {
 public:
  // Returns a single shared instance of this class. This is the typical use
  // case in Chrome.
  //
  // TODO(aa): Make this const to enforce thread-safe usage.
  static ExtensionAPI* GetSharedInstance();

  // Creates a new instance configured the way ExtensionAPI typically is in
  // Chrome. Use the default constructor to get a clean instance.
  static ExtensionAPI* CreateWithDefaultConfiguration();

  // Splits a name like "permission:bookmark" into ("permission", "bookmark").
  // The first part refers to a type of feature, for example "manifest",
  // "permission", or "api". The second part is the full name of the feature.
  //
  // TODO(kalman): ExtensionAPI isn't really the right place for this function.
  static void SplitDependencyName(const std::string& full_name,
                                  std::string* feature_type,
                                  std::string* feature_name);

  class OverrideSharedInstanceForTest {
   public:
    explicit OverrideSharedInstanceForTest(ExtensionAPI* testing_api);
    ~OverrideSharedInstanceForTest();

   private:
    raw_ptr<ExtensionAPI> original_api_;
  };

  // Creates a completely clean instance. Configure using
  // RegisterDependencyProvider before use.
  ExtensionAPI();

  ExtensionAPI(const ExtensionAPI&) = delete;
  ExtensionAPI& operator=(const ExtensionAPI&) = delete;

  virtual ~ExtensionAPI();

  // Add a FeatureProvider for APIs. The features are used to specify
  // dependencies and constraints on the availability of APIs.
  void RegisterDependencyProvider(const std::string& name,
                                  const FeatureProvider* provider);

  // Returns true if the API item called |api_full_name| and all of its
  // dependencies are available in |context|.
  //
  // |api_full_name| can be either a namespace name (like "bookmarks") or a
  // member name (like "bookmarks.create").
  //
  // Depending on the configuration of |api| (in _api_features.json), either
  // |extension| or |url| (or both) may determine its availability, but this is
  // up to the configuration of the individual feature.
  //
  // |check_alias| determines whether it should be tested whether the API
  // is available through an alias.
  //
  // TODO(kalman): This is just an unnecessary combination of finding a Feature
  // then calling Feature::IsAvailableToContext(..) on it. Just provide that
  // FindFeature function and let callers compose if they want.
  Feature::Availability IsAvailable(const std::string& api_full_name,
                                    const Extension* extension,
                                    mojom::ContextType context,
                                    const GURL& url,
                                    CheckAliasStatus check_alias,
                                    int context_id,
                                    const ContextData& context_data);

  // Determines whether an API, or any parts of that API, can be exposed to
  // |context|.
  //
  // |check_alias| determines whether it should be tested whether the API
  // is available through an alias.
  //
  bool IsAnyFeatureAvailableToContext(const Feature& api,
                                      const Extension* extension,
                                      mojom::ContextType context,
                                      const GURL& url,
                                      CheckAliasStatus check_alias,
                                      int context_id,
                                      const ContextData& context_data);

  // Gets the string_view for the schema specified by |api_name|.
  std::string_view GetSchemaStringPiece(const std::string& api_name);

  // Gets the schema for the extension API with namespace |full_name|.
  // Ownership remains with this object.
  // TODO(devlin): Now that we use GetSchemaStringPiece() in the renderer, we
  // may not really need this anymore.
  const base::Value::Dict* GetSchema(const std::string& full_name);

  // Splits a full name from the extension API into its API and child name
  // parts. Some examples:
  //
  // "bookmarks.create" -> ("bookmarks", "create")
  // "experimental.input.ui.cursorUp" -> ("experimental.input.ui", "cursorUp")
  // "storage.sync.set" -> ("storage", "sync.get")
  // "<unknown-api>.monkey" -> ("", "")
  //
  // The |child_name| parameter can be be NULL if you don't need that part.
  std::string GetAPINameFromFullName(const std::string& full_name,
                                     std::string* child_name);

  // Gets a feature from any dependency provider registered with ExtensionAPI.
  // Returns NULL if the feature could not be found.
  const Feature* GetFeatureDependency(const std::string& dependency_name);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtensionAPITest, DefaultConfigurationFeatures);
  friend struct base::DefaultSingletonTraits<ExtensionAPI>;

  void InitDefaultConfiguration();

  // Returns true if there exists an API with |name|. Declared virtual for
  // testing purposes.
  virtual bool IsKnownAPI(const std::string& name, ExtensionsClient* client);

  // Checks if |full_name| is available to provided context and extension under
  // associated API's alias name.
  Feature::Availability IsAliasAvailable(const std::string& full_name,
                                         const Feature& feature,
                                         const Extension* extension,
                                         mojom::ContextType context,
                                         const GURL& url,
                                         int context_id,
                                         const ContextData& context_data);

  // Loads a schema.
  void LoadSchema(const std::string& name, std::string_view schema);

  // Same as GetSchemaStringPiece() but doesn't acquire |lock_|.
  std::string_view GetSchemaStringPieceUnsafe(const std::string& api_name);

  // Same as GetAPINameFromFullName() but doesn't acquire |lock_|.
  std::string GetAPINameFromFullNameUnsafe(const std::string& full_name,
                                           std::string* child_name);

  bool default_configuration_initialized_ = false;

  base::Lock lock_;

  // Schemas for each namespace.
  using SchemaMap = std::map<std::string, base::Value::Dict>;
  SchemaMap schemas_ GUARDED_BY(lock_);

  // FeatureProviders used for resolving dependencies.
  using FeatureProviderMap =
      std::map<std::string, raw_ptr<const FeatureProvider, CtnExperimental>>;
  FeatureProviderMap dependency_providers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EXTENSION_API_H_
