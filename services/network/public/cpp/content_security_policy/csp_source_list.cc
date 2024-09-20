// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/content_security_policy/csp_source_list.h"

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/content_security_policy/csp_source.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"

namespace network {

using CSPDirectiveName = mojom::CSPDirectiveName;

namespace {

bool AllowFromSources(const GURL& url,
                      const std::vector<mojom::CSPSourcePtr>& sources,
                      const mojom::CSPSource& self_source,
                      bool has_followed_redirect,
                      bool is_opaque_fenced_frame) {
  for (const auto& source : sources) {
    if (CheckCSPSource(*source, url, self_source,
                       CSPSourceContext::ContentSecurityPolicy,
                       has_followed_redirect, is_opaque_fenced_frame)) {
      return true;
    }
  }
  return false;
}

// Removes from |a| elements not contained in |b|.
void IntersectNonces(base::flat_set<std::string>& a,
                     const base::flat_set<std::string>& b) {
  base::EraseIf(a, [&b](const std::string& s) { return !b.contains(s); });
}

// Removes from |a| elements not contained in |b|.
void IntersectHashes(base::flat_set<mojom::CSPHashSourcePtr>& a,
                     const base::flat_set<mojom::CSPHashSourcePtr>& b) {
  base::EraseIf(
      a, [&b](const mojom::CSPHashSourcePtr& h) { return !b.contains(h); });
}

bool IsScriptDirective(CSPDirectiveName directive) {
  return directive == CSPDirectiveName::ScriptSrc ||
         directive == CSPDirectiveName::ScriptSrcAttr ||
         directive == CSPDirectiveName::ScriptSrcElem ||
         directive == CSPDirectiveName::DefaultSrc;
}

bool IsStyleDirective(CSPDirectiveName directive) {
  return directive == CSPDirectiveName::StyleSrc ||
         directive == CSPDirectiveName::StyleSrcAttr ||
         directive == CSPDirectiveName::StyleSrcElem ||
         directive == CSPDirectiveName::DefaultSrc;
}

bool AllowAllInline(CSPDirectiveName directive,
                    const mojom::CSPSourceList& source) {
  return source.allow_inline && source.hashes.empty() &&
         source.nonces.empty() &&
         (!IsScriptDirective(directive) || !source.allow_dynamic);
}

void AddSourceSchemesToSet(base::flat_set<std::string>& set,
                           const mojom::CSPSource* source) {
  set.emplace(source->scheme);
  if (source->scheme == url::kHttpScheme)
    set.emplace(url::kHttpsScheme);
  else if (source->scheme == url::kWsScheme)
    set.emplace(url::kWssScheme);
}

base::flat_set<std::string> IntersectSchemesOnly(
    const std::vector<mojom::CSPSourcePtr>& list_a,
    const std::vector<mojom::CSPSourcePtr>& list_b) {
  base::flat_set<std::string> schemes_a;
  for (const auto& source_a : list_a) {
    if (CSPSourceIsSchemeOnly(*source_a)) {
      AddSourceSchemesToSet(schemes_a, source_a.get());
    }
  }

  base::flat_set<std::string> intersection;
  for (const auto& source_b : list_b) {
    if (CSPSourceIsSchemeOnly(*source_b)) {
      if (schemes_a.contains(source_b->scheme))
        AddSourceSchemesToSet(intersection, source_b.get());
      else if (source_b->scheme == url::kHttpScheme &&
               schemes_a.contains(url::kHttpsScheme)) {
        intersection.emplace(url::kHttpsScheme);
      } else if (source_b->scheme == url::kWsScheme &&
                 schemes_a.contains(url::kWssScheme)) {
        intersection.emplace(url::kWssScheme);
      }
    }
  }

  return intersection;
}

std::vector<mojom::CSPSourcePtr> ExpandSchemeStarAndSelf(
    const mojom::CSPSourceList& source_list,
    const mojom::CSPSource* self) {
  std::vector<mojom::CSPSourcePtr> result;
  for (const mojom::CSPSourcePtr& item : source_list.sources) {
    mojom::CSPSourcePtr new_item = item->Clone();
    if (new_item->scheme.empty()) {
      if (self && !self->scheme.empty())
        new_item->scheme = self->scheme;
    }
    result.push_back(std::move(new_item));
  }

  if (source_list.allow_star) {
    result.push_back(mojom::CSPSource::New(
        url::kFtpScheme, "", url::PORT_UNSPECIFIED, "", false, false));
    result.push_back(mojom::CSPSource::New(
        url::kWsScheme, "", url::PORT_UNSPECIFIED, "", false, false));
    result.push_back(mojom::CSPSource::New(
        url::kHttpScheme, "", url::PORT_UNSPECIFIED, "", false, false));
    if (self && !self->scheme.empty()) {
      result.push_back(mojom::CSPSource::New(
          self->scheme, "", url::PORT_UNSPECIFIED, "", false, false));
    }
  }

  if (source_list.allow_self && self && !self->scheme.empty() &&
      !self->host.empty()) {
    // If |self| is an opaque origin we should ignore it.
    result.push_back(self->Clone());
  }
  return result;
}

std::vector<mojom::CSPSourcePtr> IntersectSources(
    const mojom::CSPSourceList& source_list_a,
    const std::vector<mojom::CSPSourcePtr>& source_list_b,
    const mojom::CSPSource* self) {
  auto schemes = IntersectSchemesOnly(source_list_a.sources, source_list_b);

  std::vector<mojom::CSPSourcePtr> normalized;

  // Add all normalized scheme source expressions.
  for (const auto& it : schemes) {
    // We do not add secure versions if insecure schemes are present.
    if ((it != url::kHttpsScheme || !schemes.count(url::kHttpScheme)) &&
        (it != url::kWssScheme || !schemes.count(url::kWsScheme))) {
      normalized.emplace_back(mojom::CSPSource::New(
          it, "", url::PORT_UNSPECIFIED, "", false, false));
    }
  }

  std::vector<mojom::CSPSourcePtr> sources_a =
      ExpandSchemeStarAndSelf(source_list_a, self);
  for (auto& source_a : sources_a) {
    if (schemes.count(source_a->scheme))
      continue;

    for (auto& source_b : source_list_b) {
      // No need to add a host source expression if it is subsumed by the
      // matching scheme source expression.
      if (schemes.contains(source_b->scheme))
        continue;
      if (mojom::CSPSourcePtr local_match =
              CSPSourcesIntersect(*source_a, *source_b)) {
        normalized.emplace_back(std::move(local_match));
      }
    }
  }
  return normalized;
}

bool UrlSourceListSubsumes(
    const std::vector<mojom::CSPSourcePtr>& source_list_a,
    const std::vector<mojom::CSPSourcePtr>& source_list_b) {
  // Empty vector of CSPSources has an effect of 'none'.
  if (!source_list_a.size() || !source_list_b.size())
    return !source_list_b.size();

  // Every item in |source_list_b| must be subsumed by at least one item in
  // |source_list_a|.
  return base::ranges::all_of(source_list_b, [&](const auto& source_b) {
    return base::ranges::any_of(source_list_a, [&](const auto& source_a) {
      return CSPSourceSubsumes(*source_a, *source_b);
    });
  });
}

}  // namespace

CSPCheckResult CheckCSPSourceList(mojom::CSPDirectiveName directive_name,
                                  const mojom::CSPSourceList& source_list,
                                  const GURL& url,
                                  const mojom::CSPSource& self_source,
                                  bool has_followed_redirect,
                                  bool is_opaque_fenced_frame) {
  if (is_opaque_fenced_frame)
    DCHECK_EQ(directive_name, mojom::CSPDirectiveName::FencedFrameSrc);

  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  // Note: Opaque fenced frames only allow https urls, therefore it's fine to
  // allow '*'.
  // TODO(crbug.com/40195488): Update the return condition below if opaque
  // fenced frames can map to non-https potentially trustworthy urls to avoid
  // privacy leak.
  if (source_list.allow_star) {
    if (url.SchemeIsHTTPOrHTTPS()) {
      return CSPCheckResult::Allowed();
    }
    if (!self_source.scheme.empty() && url.SchemeIs(self_source.scheme))
      return CSPCheckResult::Allowed();
  }

  if (source_list.allow_self &&
      CheckCSPSource(self_source, url, self_source,
                     CSPSourceContext::ContentSecurityPolicy,
                     has_followed_redirect, is_opaque_fenced_frame)) {
    return CSPCheckResult::Allowed();
  }

  if (AllowFromSources(url, source_list.sources, self_source,
                       has_followed_redirect, is_opaque_fenced_frame)) {
    return CSPCheckResult::Allowed();
  }

  if (source_list.allow_star) {
    if (url.SchemeIsWSOrWSS()) {
      return CSPCheckResult::AllowedOnlyIfWildcardMatchesWs();
    }
    if (url.SchemeIs("ftp")) {
      return base::FeatureList::IsEnabled(
                 features::kCspStopMatchingWildcardDirectivesToFtp)
                 ? CSPCheckResult::Blocked()
                 : CSPCheckResult::AllowedOnlyIfWildcardMatchesFtp();
    }
  }

  return CSPCheckResult::Blocked();
}

bool CSPSourceListSubsumes(
    const mojom::CSPSourceList& source_list_a,
    const std::vector<const mojom::CSPSourceList*>& source_list_b,
    CSPDirectiveName directive,
    const mojom::CSPSource* origin_b) {
  if (source_list_b.empty())
    return false;

  auto it = source_list_b.begin();
  bool allow_inline_b = (*it)->allow_inline;
  bool allow_eval_b = (*it)->allow_eval;
  bool allow_dynamic_b = (*it)->allow_dynamic;
  bool allow_unsafe_hashes_b = (*it)->allow_unsafe_hashes;
  bool is_hash_or_nonce_present_b =
      !(*it)->nonces.empty() || !(*it)->hashes.empty();
  base::flat_set<std::string> nonces_b((*it)->nonces);
  base::flat_set<mojom::CSPHashSourcePtr> hashes_b(mojo::Clone((*it)->hashes));

  std::vector<mojom::CSPSourcePtr> normalized_sources_b =
      ExpandSchemeStarAndSelf(**it, origin_b);

  ++it;
  for (; it != source_list_b.end(); ++it) {
    // 'allow_inline' is ignored if hashes or nonces are present, or if
    // 'strict-dynamic' is specified.
    allow_inline_b = allow_inline_b && (*it)->allow_inline;
    allow_eval_b = allow_eval_b && (*it)->allow_eval;
    allow_dynamic_b = allow_dynamic_b && (*it)->allow_dynamic;
    allow_unsafe_hashes_b = allow_unsafe_hashes_b && (*it)->allow_unsafe_hashes;
    is_hash_or_nonce_present_b =
        is_hash_or_nonce_present_b &&
        (!(*it)->nonces.empty() || !(*it)->hashes.empty());
    base::flat_set<std::string> item_nonces((*it)->nonces);
    IntersectNonces(nonces_b, item_nonces);
    base::flat_set<mojom::CSPHashSourcePtr> item_hashes(
        mojo::Clone((*it)->hashes));
    IntersectHashes(hashes_b, item_hashes);
    normalized_sources_b =
        IntersectSources(**it, normalized_sources_b, origin_b);
  }

  // If source_list_b enforces some nonce, then source_list_a must contain
  // some nonce, but they do not need to match.
  if (!nonces_b.empty() && source_list_a.nonces.empty())
    return false;

  // All hashes enforced by source_list_b must be contained in source_list_a.
  if (!hashes_b.empty()) {
    base::flat_set<mojom::CSPHashSourcePtr> hashes_a(
        mojo::Clone(source_list_a.hashes));
    for (const auto& hash : hashes_b) {
      if (!hashes_a.count(hash))
        return false;
    }
  }

  if (IsScriptDirective(directive) || IsStyleDirective(directive)) {
    if (!source_list_a.allow_eval && allow_eval_b)
      return false;
    if (!source_list_a.allow_unsafe_hashes && allow_unsafe_hashes_b)
      return false;

    bool allow_all_inline_b =
        allow_inline_b && !is_hash_or_nonce_present_b &&
        (!IsScriptDirective(directive) || !allow_dynamic_b);
    if (!AllowAllInline(directive, source_list_a) && allow_all_inline_b)
      return false;
  }

  if (IsScriptDirective(directive) &&
      (source_list_a.allow_dynamic || allow_dynamic_b)) {
    // If `this` does not allow `strict-dynamic`, then it must be that `other`
    // does allow, so the result is `false`.
    if (!source_list_a.allow_dynamic)
      return false;
    // All keyword source expressions have been considered so only CSPSource
    // subsumption is left. However, `strict-dynamic` ignores all CSPSources so
    // for subsumption to be true either `other` must allow `strict-dynamic` or
    // have no allowed CSPSources.
    return allow_dynamic_b || !normalized_sources_b.size();
  }

  // If embedding CSP specifies `self`, `self` refers to the embedee's origin.
  std::vector<mojom::CSPSourcePtr> normalized_sources_a =
      ExpandSchemeStarAndSelf(source_list_a, origin_b);
  return UrlSourceListSubsumes(normalized_sources_a, normalized_sources_b);
}

std::string ToString(const mojom::CSPSourceListPtr& source_list) {
  bool is_none = !source_list->allow_self && !source_list->allow_star &&
                 source_list->sources.empty();
  if (is_none)
    return "'none'";
  if (source_list->allow_star)
    return "*";

  bool is_empty = true;
  std::stringstream text;
  if (source_list->allow_self) {
    text << "'self'";
    is_empty = false;
  }

  for (const auto& source : source_list->sources) {
    if (!is_empty)
      text << " ";
    text << ToString(*source);
    is_empty = false;
  }

  return text.str();
}

}  // namespace network
