// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/pickle.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/user_script.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

static const int kAllSchemes =
    URLPattern::SCHEME_HTTP |
    URLPattern::SCHEME_HTTPS |
    URLPattern::SCHEME_FILE |
    URLPattern::SCHEME_FTP |
    URLPattern::SCHEME_CHROMEUI;

TEST(ExtensionUserScriptTest, Glob_HostString) {
  UserScript script;
  script.add_glob("*mail.google.com*");
  script.add_glob("*mail.yahoo.com*");
  script.add_glob("*mail.msn.com*");
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.google.com")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesURL(GURL("https://mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesURL(GURL("ftp://mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://woo.mail.google.com/foo")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.yahoo.com/bar")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.msn.com/baz")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://www.hotmail.com")));

  script.add_exclude_glob("*foo*");
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.google.com")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://mail.google.com/foo")));
}

TEST(ExtensionUserScriptTest, Glob_TrailingSlash) {
  UserScript script;
  script.add_glob("*mail.google.com/");
  // GURL normalizes the URL to have a trailing "/"
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.google.com")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.google.com/")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://mail.google.com/foo")));
}

TEST(ExtensionUserScriptTest, Glob_TrailingSlashStar) {
  UserScript script;
  script.add_glob("http://mail.google.com/*");
  // GURL normalizes the URL to have a trailing "/"
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.google.com")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://mail.google.com/foo")));
  EXPECT_FALSE(script.MatchesURL(GURL("https://mail.google.com/foo")));
}

TEST(ExtensionUserScriptTest, Glob_Star) {
  UserScript script;
  script.add_glob("*");
  EXPECT_TRUE(script.MatchesURL(GURL("http://foo.com/bar")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://hot.com/dog")));
  EXPECT_TRUE(script.MatchesURL(GURL("https://hot.com/dog")));
  EXPECT_TRUE(script.MatchesURL(GURL("file:///foo/bar")));
  EXPECT_TRUE(script.MatchesURL(GURL("file://localhost/foo/bar")));
}

TEST(ExtensionUserScriptTest, Glob_StringAnywhere) {
  UserScript script;
  script.add_glob("*foo*");
  EXPECT_TRUE(script.MatchesURL(GURL("http://foo.com/bar")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://baz.org/foo/bar")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://baz.org")));
}

TEST(ExtensionUserScriptTest, UrlPattern) {
  URLPattern pattern(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("http://*/foo*"));

  UserScript script;
  script.add_url_pattern(pattern);
  EXPECT_TRUE(script.MatchesURL(GURL("http://monkey.com/foobar")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://monkey.com/hotdog")));

  // NOTE: URLPattern is tested more extensively in url_pattern_unittest.cc.
}

TEST(ExtensionUserScriptTest, ExcludeUrlPattern) {
  UserScript script;

  URLPattern pattern(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://*.nytimes.com/*"));
  script.add_url_pattern(pattern);

  URLPattern exclude(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess,
            exclude.Parse("*://*/*business*"));
  script.add_exclude_url_pattern(exclude);

  EXPECT_TRUE(script.MatchesURL(GURL("http://www.nytimes.com/health")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://www.nytimes.com/business")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://business.nytimes.com")));
}

TEST(ExtensionUserScriptTest, ExcludeUrlPatternWithTrailingDot) {
  UserScript script;

  URLPattern pattern(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse("*://*/*"));
  script.add_url_pattern(pattern);

  URLPattern exclude(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess,
            exclude.Parse("*://mail.nytimes.com/*"));
  script.add_exclude_url_pattern(exclude);

  EXPECT_TRUE(script.MatchesURL(GURL("http://www.nytimes.com/health")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://business.nytimes.com")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://mail.nytimes.com")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://mail.nytimes.com.")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://mail.nytimes.com/login")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://mail.nytimes.com./login")));
}

TEST(ExtensionUserScriptTest, UrlPatternAndIncludeGlobs) {
  UserScript script;

  URLPattern pattern(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://*.nytimes.com/*"));
  script.add_url_pattern(pattern);

  script.add_glob("*nytimes.com/???s/*");

  EXPECT_TRUE(script.MatchesURL(GURL("http://www.nytimes.com/arts/1.html")));
  EXPECT_TRUE(script.MatchesURL(GURL("http://www.nytimes.com/jobs/1.html")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://www.nytimes.com/sports/1.html")));
}

TEST(ExtensionUserScriptTest, UrlPatternAndExcludeGlobs) {
  UserScript script;

  URLPattern pattern(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://*.nytimes.com/*"));
  script.add_url_pattern(pattern);

  script.add_exclude_glob("*science*");

  EXPECT_TRUE(script.MatchesURL(GURL("http://www.nytimes.com")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://science.nytimes.com")));
  EXPECT_FALSE(script.MatchesURL(GURL("http://www.nytimes.com/science")));
}

TEST(ExtensionUserScriptTest, UrlPatternGlobInteraction) {
  // If there are both, match intersection(union(globs), union(urlpatterns)).
  UserScript script;

  URLPattern pattern(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess,
            pattern.Parse("http://www.google.com/*"));
  script.add_url_pattern(pattern);

  script.add_glob("*bar*");

  // No match, because it doesn't match the glob.
  EXPECT_FALSE(script.MatchesURL(GURL("http://www.google.com/foo")));

  script.add_exclude_glob("*baz*");

  // No match, because it matches the exclude glob.
  EXPECT_FALSE(script.MatchesURL(GURL("http://www.google.com/baz")));

  // Match, because it matches the glob, doesn't match the exclude glob.
  EXPECT_TRUE(script.MatchesURL(GURL("http://www.google.com/bar")));

  // Try with just a single exclude glob.
  script.clear_globs();
  EXPECT_TRUE(script.MatchesURL(GURL("http://www.google.com/foo")));

  // Try with no globs or exclude globs.
  script.clear_exclude_globs();
  EXPECT_TRUE(script.MatchesURL(GURL("http://www.google.com/foo")));
}

TEST(ExtensionUserScriptTest, Pickle) {
  URLPattern pattern1(kAllSchemes);
  URLPattern pattern2(kAllSchemes);
  URLPattern exclude1(kAllSchemes);
  URLPattern exclude2(kAllSchemes);
  ASSERT_EQ(URLPattern::ParseResult::kSuccess, pattern1.Parse("http://*/foo*"));
  ASSERT_EQ(URLPattern::ParseResult::kSuccess,
            pattern2.Parse("http://bar/baz*"));
  ASSERT_EQ(URLPattern::ParseResult::kSuccess, exclude1.Parse("*://*/*bar"));
  ASSERT_EQ(URLPattern::ParseResult::kSuccess, exclude2.Parse("https://*/*"));

  UserScript script1;
  script1.js_scripts().push_back(std::make_unique<UserScript::File>(
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\")),
      base::FilePath(FILE_PATH_LITERAL("foo.user.js")),
      GURL("chrome-extension://abc/foo.user.js")));
  script1.css_scripts().push_back(std::make_unique<UserScript::File>(
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\")),
      base::FilePath(FILE_PATH_LITERAL("foo.user.css")),
      GURL("chrome-extension://abc/foo.user.css")));
  script1.css_scripts().push_back(std::make_unique<UserScript::File>(
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\")),
      base::FilePath(FILE_PATH_LITERAL("foo2.user.css")),
      GURL("chrome-extension://abc/foo2.user.css")));
  script1.set_run_location(mojom::RunLocation::kDocumentStart);

  script1.add_url_pattern(pattern1);
  script1.add_url_pattern(pattern2);
  script1.add_exclude_url_pattern(exclude1);
  script1.add_exclude_url_pattern(exclude2);

  const std::string kId = "_mc_12";
  script1.set_id(kId);
  const std::string kExtensionId = "foo";
  mojom::HostID id(mojom::HostID::HostType::kExtensions, kExtensionId);
  script1.set_host_id(id);

  base::Pickle pickle;
  script1.Pickle(&pickle);

  base::PickleIterator iter(pickle);
  UserScript script2;
  script2.Unpickle(pickle, &iter);

  EXPECT_EQ(1U, script2.js_scripts().size());
  EXPECT_EQ(script1.js_scripts()[0]->url(), script2.js_scripts()[0]->url());

  EXPECT_EQ(2U, script2.css_scripts().size());
  for (size_t i = 0; i < script2.js_scripts().size(); ++i) {
    EXPECT_EQ(script1.css_scripts()[i]->url(), script2.css_scripts()[i]->url());
  }

  ASSERT_EQ(script1.globs().size(), script2.globs().size());
  for (size_t i = 0; i < script1.globs().size(); ++i) {
    EXPECT_EQ(script1.globs()[i], script2.globs()[i]);
  }

  ASSERT_EQ(script1.url_patterns(), script2.url_patterns());
  ASSERT_EQ(script1.exclude_url_patterns(), script2.exclude_url_patterns());

  EXPECT_EQ(kExtensionId, script2.extension_id());
  EXPECT_EQ(kId, script2.id());
}

TEST(ExtensionUserScriptTest, Defaults) {
  UserScript script;
  ASSERT_EQ(mojom::RunLocation::kDocumentIdle, script.run_location());
}

}  // namespace extensions
