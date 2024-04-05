// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_USER_SCRIPT_H_
#define EXTENSIONS_COMMON_USER_SCRIPT_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/execution_world.mojom-shared.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/script_constants.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

namespace base {
class Pickle;
class PickleIterator;
}

namespace extensions {

// Represents a user script, either a standalone one, or one that is part of an
// extension.
class UserScript {
 public:
  // Denotes the type/origin of this script.
  enum class Source {
    // The script was parsed from an extension's manifest entry.
    kStaticContentScript,

    // The script was created through the scripting API.
    kDynamicContentScript,

    // The script was created through the userScripts API.
    kDynamicUserScript,

    // The script was created for a webUI.
    kWebUIScript,
  };

  // The file extension for standalone user scripts.
  static constexpr const char kFileExtension[] = "user.js";

  // The first character for all user script IDs. IDs provided by an extension
  // should never start with this character.
  static constexpr const char kReservedScriptIDPrefix = '_';

  // The prefix for all manifest content script IDs and IDs generated through
  // `GenerateUserScriptID`.
  static constexpr const char kManifestContentScriptPrefix[] = "_mc_";

  // The prefix for all dynamic content scripts registered through the scripting
  // API.
  static constexpr const char kDynamicContentScriptPrefix[] = "_dc_";

  // The prefix for all user scripts registered through the userScripts API.
  static constexpr const char kDynamicUserScriptPrefix[] = "_du_";

  static std::string GenerateUserScriptID();

  // Removes any appended prefix from the given `script_id`.
  static std::string TrimPrefixFromScriptID(const std::string& script_id);

  // Returns the source of `script_id`. This can only be called if `script_id`
  // already has its source prefix appended.
  static Source GetSourceForScriptID(const std::string& script_id);

  // Check if a URL should be treated as a user script and converted to an
  // extension.
  static bool IsURLUserScript(const GURL& url, const std::string& mime_type);

  // Get the valid user script schemes for the current process. If
  // `can_execute_script_everywhere` is true, this will return ALL_SCHEMES.
  static int ValidUserScriptSchemes(bool can_execute_script_everywhere = false);

  // Holds the script content.
  class Content {
   public:
    // Source of the script content.
    enum class Source { kFile, kInlineCode };

    Content();
    Content(const Content& other);
    ~Content();

    // Creates a content object with kFile source. It store the URL where the
    // file will be fetched from.
    static std::unique_ptr<Content> CreateFile(
        const base::FilePath& extension_root,
        const base::FilePath& relative_path,
        const GURL& url);
    // Creates a content object with kInlineCode source. `url` must be unique,
    // since it's used to inject a js script. This means that a unique url has
    // to be generated, since inline code scripts don't have a url.
    static std::unique_ptr<Content> CreateInlineCode(const GURL& url);

    Source source() { return source_; }

    const base::FilePath& extension_root() const { return extension_root_; }
    const base::FilePath& relative_path() const { return relative_path_; }

    const GURL& url() const { return url_; }
    void set_url(const GURL& url) { url_ = url; }

    // If external_content_ is set returns it as content otherwise it returns
    // content_
    const std::string_view GetContent() const {
      if (external_content_.data())
        return external_content_;
      else
        return content_;
    }
    void set_external_content(std::string_view content) {
      external_content_ = content;
    }
    void set_content(std::string content) { content_ = std::move(content); }

    // Serialization support. The content and FilePath members will not be
    // serialized!
    void Pickle(base::Pickle* pickle) const;
    void Unpickle(const base::Pickle& pickle, base::PickleIterator* iter);

   private:
    Content(Source source,
            const base::FilePath& extension_root,
            const base::FilePath& relative_path,
            const GURL& url);

    // The source of the script.
    Source source_;

    // Where the script file lives on the disk. We keep the path split so that
    // it can be localized at will.
    base::FilePath extension_root_;
    base::FilePath relative_path_;

    // The url to this script file.
    GURL url_;

    // The script content. It can be set to either loaded_content_ or
    // externally allocated string.
    std::string_view external_content_;

    // Set when the content is loaded by LoadContent
    std::string content_;
  };

  using ContentList = std::vector<std::unique_ptr<Content>>;

  // Type of a API consumer instance that user scripts will be injected on.
  enum ConsumerInstanceType { TAB, WEBVIEW };

  // Constructor. Default the run location to document end, which is like
  // Greasemonkey and probably more useful for typical scripts.
  UserScript();

  UserScript(const UserScript&) = delete;
  UserScript& operator=(const UserScript&) = delete;

  ~UserScript();

  // Performs a copy of all fields except file contents.
  static std::unique_ptr<UserScript> CopyMetadataFrom(const UserScript& other);

  const std::string& name_space() const { return name_space_; }
  void set_name_space(const std::string& name_space) {
    name_space_ = name_space;
  }

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  const std::string& version() const { return version_; }
  void set_version(const std::string& version) {
    version_ = version;
  }

  const std::string& description() const { return description_; }
  void set_description(const std::string& description) {
    description_ = description;
  }

  // The place in the document to run the script.
  mojom::RunLocation run_location() const { return run_location_; }
  void set_run_location(mojom::RunLocation location) {
    run_location_ = location;
  }

  // Whether to emulate greasemonkey when running this script.
  bool emulate_greasemonkey() const { return emulate_greasemonkey_; }
  void set_emulate_greasemonkey(bool val) { emulate_greasemonkey_ = val; }

  // Whether to match all frames, or only the top one.
  bool match_all_frames() const { return match_all_frames_; }
  void set_match_all_frames(bool val) { match_all_frames_ = val; }

  // Whether to match the origin as a fallback if the URL cannot be used
  // directly.
  MatchOriginAsFallbackBehavior match_origin_as_fallback() const {
    return match_origin_as_fallback_;
  }
  void set_match_origin_as_fallback(MatchOriginAsFallbackBehavior val) {
    match_origin_as_fallback_ = val;
  }

  // The globs, if any, that determine which pages this script runs against.
  // These are only used with "standalone" Greasemonkey-like user scripts.
  const std::vector<std::string>& globs() const { return globs_; }
  void add_glob(const std::string& glob) { globs_.push_back(glob); }
  void clear_globs() { globs_.clear(); }
  const std::vector<std::string>& exclude_globs() const {
    return exclude_globs_;
  }
  void add_exclude_glob(const std::string& glob) {
    exclude_globs_.push_back(glob);
  }
  void clear_exclude_globs() { exclude_globs_.clear(); }

  // The URLPatterns, if any, that determine which pages this script runs
  // against.
  const URLPatternSet& url_patterns() const { return url_set_; }
  void add_url_pattern(const URLPattern& pattern);
  const URLPatternSet& exclude_url_patterns() const {
    return exclude_url_set_;
  }
  void add_exclude_url_pattern(const URLPattern& pattern);

  // List of js scripts for this user script
  ContentList& js_scripts() { return js_scripts_; }
  const ContentList& js_scripts() const { return js_scripts_; }

  // List of css scripts for this user script
  ContentList& css_scripts() { return css_scripts_; }
  const ContentList& css_scripts() const { return css_scripts_; }

  const ExtensionId& extension_id() const { return host_id_.id; }

  const mojom::HostID& host_id() const { return host_id_; }
  void set_host_id(const mojom::HostID& host_id) { host_id_ = host_id; }

  const ConsumerInstanceType& consumer_instance_type() const {
    return consumer_instance_type_;
  }
  void set_consumer_instance_type(
      const ConsumerInstanceType& consumer_instance_type) {
    consumer_instance_type_ = consumer_instance_type;
  }

  const std::string& id() const { return user_script_id_; }
  void set_id(std::string id) { user_script_id_ = std::move(id); }

  // TODO(lazyboy): Incognito information is extension specific, it doesn't
  // belong here. We should be able to determine this in the renderer/ where it
  // is used.
  bool is_incognito_enabled() const { return incognito_enabled_; }
  void set_incognito_enabled(bool enabled) { incognito_enabled_ = enabled; }

  mojom::ExecutionWorld execution_world() const { return execution_world_; }
  void set_execution_world(mojom::ExecutionWorld world) {
    execution_world_ = world;
  }

  const std::optional<std::string>& world_id() const { return world_id_; }
  void set_world_id(std::optional<std::string> world_id) {
    world_id_ = std::move(world_id);
  }

  // Returns the script's ID without the appended prefix.
  std::string GetIDWithoutPrefix() const;

  // Returns the type of this script, which is derived from examining the
  // HostID and the prefix of the ID.
  Source GetSource() const;

  // Returns true if the script should be applied to the specified URL, false
  // otherwise.
  bool MatchesURL(const GURL& url) const;

  // Returns true if the script should be applied to the given
  // |effective_document_url|. It is the caller's responsibility to calculate
  // |effective_document_url| based on match_origin_as_fallback().
  bool MatchesDocument(const GURL& effective_document_url,
                       bool is_subframe) const;

  // Serializes the UserScript into a pickle. The content of the scripts and
  // paths to UserScript::Content will not be serialized!
  void Pickle(base::Pickle* pickle) const;

  // Deserializes the script from a pickle. Note that this always succeeds
  // because presumably we were the one that pickled it, and we did it
  // correctly.
  void Unpickle(const base::Pickle& pickle, base::PickleIterator* iter);

 private:
  // base::Pickle helper functions used to pickle the individual types of
  // components.
  void PickleGlobs(base::Pickle* pickle,
                   const std::vector<std::string>& globs) const;
  void PickleHostID(base::Pickle* pickle, const mojom::HostID& host_id) const;
  void PickleURLPatternSet(base::Pickle* pickle,
                           const URLPatternSet& pattern_list) const;
  void PickleScripts(base::Pickle* pickle, const ContentList& scripts) const;

  // Unpickle helper functions used to unpickle individual types of components.
  void UnpickleGlobs(const base::Pickle& pickle,
                     base::PickleIterator* iter,
                     std::vector<std::string>* globs);
  void UnpickleHostID(const base::Pickle& pickle,
                      base::PickleIterator* iter,
                      mojom::HostID* host_id);
  void UnpickleURLPatternSet(const base::Pickle& pickle,
                             base::PickleIterator* iter,
                             URLPatternSet* pattern_list);
  void UnpickleScripts(const base::Pickle& pickle,
                       base::PickleIterator* iter,
                       ContentList* scripts);

  // The location to run the script inside the document.
  mojom::RunLocation run_location_ = mojom::RunLocation::kDocumentIdle;

  // The namespace of the script. This is used by Greasemonkey in the same way
  // as XML namespaces. Only used when parsing Greasemonkey-style scripts.
  std::string name_space_;

  // The script's name. Only used when parsing Greasemonkey-style scripts.
  std::string name_;

  // A longer description. Only used when parsing Greasemonkey-style scripts.
  std::string description_;

  // A version number of the script. Only used when parsing Greasemonkey-style
  // scripts.
  std::string version_;

  // Greasemonkey-style globs that determine pages to inject the script into.
  // These are only used with standalone scripts.
  std::vector<std::string> globs_;
  std::vector<std::string> exclude_globs_;

  // URLPatterns that determine pages to inject the script into. These are
  // only used with scripts that are part of extensions.
  URLPatternSet url_set_;
  URLPatternSet exclude_url_set_;

  // List of js scripts defined in content_scripts
  ContentList js_scripts_;

  // List of css scripts defined in content_scripts
  ContentList css_scripts_;

  // The ID of the host this script is a part of. The |ID| of the
  // |host_id| can be empty if the script is a "standlone" user script.
  mojom::HostID host_id_;

  // The type of the consumer instance that the script will be injected.
  ConsumerInstanceType consumer_instance_type_ = TAB;

  // The globally-unique id associated with this user script. An empty string
  // indicates an invalid id.
  std::string user_script_id_;

  // Whether we should try to emulate Greasemonkey's APIs when running this
  // script.
  bool emulate_greasemonkey_ = false;

  // Whether the user script should run in all frames, or only just the top one.
  bool match_all_frames_ = false;

  // Whether the user script should run in frames whose initiator / precursor
  // origin matches a match pattern, if an appropriate URL cannot be found for
  // the frame for matching purposes, such as in the case of about:, data:, and
  // other schemes.
  MatchOriginAsFallbackBehavior match_origin_as_fallback_ =
      MatchOriginAsFallbackBehavior::kNever;

  // True if the script should be injected into an incognito tab.
  bool incognito_enabled_ = false;

  mojom::ExecutionWorld execution_world_ = mojom::ExecutionWorld::kIsolated;

  // The ID of the unique world into which to inject, if any. If omitted, uses
  // the default world for the given `execution_world_` type.
  std::optional<std::string> world_id_;
};

using UserScriptList = std::vector<std::unique_ptr<UserScript>>;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_USER_SCRIPT_H_
