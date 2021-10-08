# Declarative Net Request

This doc gives a brief overview of the implementation of the
`declarativeNetRequest` API which allows extensions to specify declarative rules
to block, redirect, upgrade or modify headers on a network request.


## Rule Indexing

[Declarative Net
Request](https://developer.chrome.com/docs/extensions/reference/declarativeNetRequest/)
consumes JSON rules from extensions and evaluates them in the browser on the
extension's behalf. Before these rules can be evaluated, they are indexed into
the [flatbuffer](https://google.github.io/flatbuffers/) format. See
[extension\_ruleset.fbs](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/flat/extension_ruleset.fbs?q=extension_ruleset.fbs)
for the flatbuffer schema we use.

First a JSON
[Rule](https://developer.chrome.com/docs/extensions/reference/declarativeNetRequest/#type-Rule)
is parsed into a `base::Value`. Since reading untrustworthy JSON using C++ in a
privileged process like the browser is dangerous as per the [Rule of
2](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/security/rule-of-2.md),
we do this either in the renderer (for dynamic and session-scoped rule APIs
where extensions specify rules as part of API calls), or in a sandboxed process
for static rules. (For unpacked extensions, this is still done in the browser
process, see
[crbug.com/761107](https://bugs.chromium.org/p/chromium/issues/detail?id=761107)).

The `base::Value` is then parsed into the
`extensions::api::declarative_net_request::Rule` struct. For static rules, this
happens in the browser and any rules which fail to be parsed correctly are
ignored to maintain backwards (consider an extension written for a newer browser
version working on an older browser) and forwards (consider an extension written
for an old browser version working on a newer version) compatibility. For
dynamic and session-scoped rules, the renderers ensure that only correctly
specified JSON rules are passed to the browser, else an API error is raised.

The parsed rule is then converted to an intermediate
<code>[IndexedRule](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/indexed_rule.h)</code>
struct. Any failures here correspond to incorrectly specified rules. These
failures result in an error for dynamic and session rules APIs. However, such
failures are ignored for packed extensions where rulesets are indexed lazily.

Finally the `IndexedRule` is indexed as a flatbuffer
<code>[UrlRule](https://source.chromium.org/chromium/chromium/src/+/main:components/url_pattern_index/flat/url_pattern_index.fbs;l=82;bpv=1;bpt=0;drc=58b5cd9b129b1b8465c040e52ad4cbfef9f8265b)</code>.

Indexing rulesets in this way provides for more efficient matching since we are
able to utilize the
<code>[url_pattern_index](https://source.chromium.org/chromium/chromium/src/+/main:components/url_pattern_index/)</code>
component which does pre-computation to allow for fast matching of filter-list
style rules. Another benefit to using the flatbuffer format is that we can load
these rules from disk with minimal deserialization (excess work is only needed
for regex-style rules which are not handled by the
<code>url_pattern_index</code> component). This ensures that the browser is not
slowed down at startup when extensions utilizing `declarativeNetRequest` are
loaded.


## Rulesets

Each extension can specify static, dynamic, and session-scoped rules.

*   Static rulesets are specified as JSON files and are part of the extension
    package. These can be specified via the manifest to be enabled or disabled
    by default. These can then be selectively enabled or disabled at runtime
    using the
    [updateEnabledRulesets](http://localhost:8080/docs/extensions/reference/declarativeNetRequest/#method-updateEnabledRulesets)
    API.
*   Dynamic rules are persisted across different sessions and can be updated
    using the
    [updateDynamicRules](http://localhost:8080/docs/extensions/reference/declarativeNetRequest/#method-updateDynamicRules)
    API.
*   Session-scoped rules are scoped to a single session and can be updated using
    the
    [updateSessionRules](http://localhost:8080/docs/extensions/reference/declarativeNetRequest/#method-updateSessionRules)
    API.

Hence, an extension can have multiple rulesets (N different static rulesets and
1 each dynamic and session-scoped ruleset). A single Ruleset source is
represented in code as a
<code>[RulesetSource](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/ruleset_source.h)</code>
which provides utilities to index the ruleset and to load it for further
matching. A <code>RulesetSource</code> can be file-backed (represented as
<code>[FileBackedRulesetSource](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h)</code>
in code) or not.

*   Session-scoped rulesets are never persisted to disk and are entirely backed
    in memory.
*   Static rulesets are file backed. Static JSON rulesets are part of the
    original extension package and their path is specified via the manifest.
    Static indexed rulesets are also persisted within the extension directory by
    Chrome under the Chrome-reserved `_metadata` folder.
*   The dynamic ruleset is also file backed. The corresponding JSON and indexed
    rulesets are stored under the `$profile_path/DNR Extension
    Rules/$extension_id/` directory path wth `rules.json` and `rules.fbs` file
    names respectively.

A static ruleset is immutable (an extension can’t change the constituent rules).
Static rulesets are indexed in a lazy manner as needed (for packed extensions)
while dynamic and session-scoped rulesets are reindexed each time they are
modified.


## Loading indexed rulesets for matching

When an extension is enabled, all its rulesets must be loaded (from disk for
static/dynamic rulesets and from memory for the session-scoped ruleset) for the
extension to work. Similarly, when an extension is disabled, all its rulesets
must be disabled as well. Additionally, when an extension is uninstalled, all
its rulesets must be deleted (from disk/memory).

<code>[RulesMonitorService](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/rules_monitor_service.h)</code>
is the profile-bound class that observes extension
loading/unloading/uninstallation and des all this work. It uses the
<code>[FileSequenceHelper](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/file_sequence_helper.h)</code>
class to aid in file-bound operations like loading rulesets from disk.
Declarative Net Request extension function implementations further forward the
implementation to the <code>RulesMonitorService</code> in most cases as well.


## Ruleset Integrity

For dynamic and static rules, the indexed and JSON rulesets are loaded from
disk. On-disk modification of resources is outside Chrome’s security model,
however we do try to ensure integrity on a best effort basis.

To ensure integrity of the indexed rulesets, Chrome maintains expected checksums
for the indexed rulesets in extension prefs which are verified prior to loading
the indexed ruleset. Additionally, flatbuffer provides utilities to ensure that
the serialized data does indeed correspond to a flatbuffer indexed blob of the
correct schema.

If loading the indexed ruleset fails (due to failed integrity checks or some
other reason), we try to reindex the JSON ruleset on disk. If even the
reindexing fails, we don't load that ruleset and a user-visible warning is
raised using
<code>[WarningService](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/warning_service.h)</code>.

We don’t have any explicit integrity checks for the JSON ruleset on disk. (There
is a [bug](https://bugs.chromium.org/p/chromium/issues/detail?id=1063206) to
ensure static JON rulesets undergo content verification since they are part of
the extension package). However, we do ensure that a reindexed ruleset still
corresponds to the last recorded checksum, thereby ensuring integrity by proxy.
(For example, if we were to change both the JSON and indexed dynamic ruleset on
disk, the dynamic ruleset will fail to load, leading to a user visible warning).


## Indexed Ruleset Schema Versioning

As mentioned previously, a file backed indexed ruleset can fail to load due to
multiple reasons (failed integrity checks, file deleted from disk, etc.). When
this happens, Chrome reindexes the JSON ruleset on disk.

Additionally, as the API evolves, the indexed flatbuffer schema format also
changes, sometimes in a backwards incompatible manner. Chrome cannot handle
indexed rulesets with an incompatible schema. To handle this, we maintain the
indexed ruleset format version in Chrome and also persist it to disk as part of
the indexed ruleset. See calls to
<code>[GetIndexedRulesetFormatVersion()](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/utils.cc;l=70;drc=58b5cd9b129b1b8465c040e52ad4cbfef9f8265b;bpv=1;bpt=1)</code>.
This indexed ruleset format version is incremented each time the flatbuffer
schema changes.

Upon reading an indexed ruleset, Chrome parses the version header from the file
and compares it to the version it understands. In case of a mismatch, the
indexed ruleset is reindexed. Note that Chrome ignores checksum mismatch in this
case, since on a schema update it’s expected for the indexed ruleset checksum to
change. It also updates the checksum stored in extension prefs.


## Rule Matching

There are several classes which aid in rule matching. At the top, we have the
`RulesetManager` class which is owned by `RulesMonitorService`.

The
<code>[RulesetManager](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/ruleset_manager.h)</code>
class manages the set of active rulesets across all enabled extensions. This
class is the entry-point for all our rule matching logic.

The active rulesets for a single extension are represented by the
<code>[CompositeMatcher](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/composite_matcher.h)</code>
class. These are owned by <code>RulesetManager</code>.

A single `RulesetSource` when loaded (and ready for matching) corresponds to a
<code>[RulesetMatcher](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/ruleset_matcher.h)</code>.
These are owned by the corresponding <code>CompositeMatcher</code> for the
extension. A <code>RulesetMatcher</code> further owns one instance each of
<code>RegexRulesMatcher</code> and <code>ExtensionUrlPatternIndexMatcher</code>.

Hence if an extension has 5 static rulesets and also uses session-scoped rules,
it will have a single `CompositeMatcher` with 6 `RulesetMatchers` (one for the
session-scoped ruleset and 5 for static rulesets).

<code>[RegexRulesMatcher](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/regex_rules_matcher.h)</code>
deals with the matching of regular expression rules within a Ruleset. This uses
the
<code>[FilteredRE2](https://source.chromium.org/chromium/chromium/src/+/main:third_party/re2/src/re2/filtered_re2.h)</code>
class from the <code>[re2](https://github.com/google/re2)</code> library for
efficient matching.

<code>[ExtensionUrlPatternIndexMatcher](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/extension_url_pattern_index_matcher.h)</code>
deals with the matching of filter-list style rules. This further uses the
<code>url_pattern_index</code> component for efficient matching (which we share
with the
<code>[subresource_filter](https://source.chromium.org/chromium/chromium/src/+/main:components/subresource_filter)</code>
component). Both `RegexRulesMatcher` and `ExtensionUrlPatternIndexMatcher` share
a common base class called
<code>[RulesetMatcherBase](https://source.chromium.org/chromium/chromium/src/+/main:extensions/browser/api/declarative_net_request/ruleset_matcher_base.h)</code>
to share logic.


## Threading

The `declarativeNetRequest` API operates mostly on the UI thread in the browser
(as does most browser communication with the network service), with the
exception of some ruleset indexing and loading operations which happen on the
file sequence. Additionally, the static JSON rulesets for packed extensions are
read in a sandboxed process, as discussed previously.


## Rule Matching algorithm

Before a network request is sent to the server, each extension is queried for an
action to take. The following actions are considered at this stage:

*   Actions which block requests of type `block`
*   Actions which redirect requests of type `redirect` or `upgradeScheme`
*   Actions which allow requests of type `allow` or `allowAllRequests`

If more than one extension returns an action, the extension whose action type
comes first in the list above gets priority. If more than one extension returns
an action with the same priority (position in the list), the most recently
installed extension gets priority.

When an extension is queried for how to handle a request, the highest priority
matching rule is returned. If more than one matching rule has the highest
priority, the tie is broken based on the action type, in the following order of
decreasing precedence:

*   `allow`
*   `allowAllRequests`
*   `block`
*   `upgradeScheme`
*   `redirect`

If the request was not blocked or redirected, the matching `modifyHeaders` rules
are evaluated with the most recently installed extensions getting priority.
Within each extension, all `modifyHeaders` rules with a priority lower than
matching `allow` or `allowAllRequests` rules are ignored.


## Interaction with webRequest API

Declarative Net Request actions are calculated during the
<code>[onBeforeRequest](https://developer.chrome.com/docs/extensions/reference/webRequest/#event-onBeforeRequest)</code>
stage of the `webRequest` API i.e. before any TCP connection is made with the
server. Actions to block, collapse, upgrade or redirect the request are applied
at this stage while header modification actions are calculated but not applied
yet (there are no headers to apply the actions to at this stage). This happens
before webRequest extensions get a chance to intercept the request, thereby
giving extensions using `declarativeNetRequest` more priority over those using
`webRequest`.

If the request was not blocked or redirected, the request proceeds. During the
<code>[onBeforeSendHeaders](https://developer.chrome.com/docs/extensions/reference/webRequest/#event-onBeforeSendHeaders)</code>
stage, after the initial set of request headers to be sent to the server have
been prepared, webRequest extensions are given a chance to propose request
header modifications. After the browser receives the response from all
`onBeforeSendHeaders` listeners, the request header modifications are applied.
Modifications proposed by `declarativeNetRequest` extensions are given priority
over those requested by `webRequest` extensions.

During the
<code>[onHeadersReceived](https://developer.chrome.com/docs/extensions/reference/webRequest/#event-onHeadersReceived)</code>
stage of the request, corresponding webRequest listeners are fired which can
propose response header modifications. After the browser receives the response
from all `onHeadersReceived` listeners, the response header modifications are
applied. Modifications proposed by `declarativeNetRequest` extensions are again
given priority over those requested by `webRequest` extensions.
