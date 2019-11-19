/*
 * \copyright Copyright 2013 Google Inc. All Rights Reserved.
 * \license @{
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @}
 */

#include "net/third_party/uri_template/uri_template.h"

#include <memory>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace uri_template {
namespace {

std::unordered_map<string, string> parameters_ = {
    {"var", "value"},
    {"hello", "Hello World!"},
    {"path", "/foo/bar"},
    {"empty", ""},
    {"x", "1024"},
    {"y", "768"},
    {"percent", "%31"},
    {"bad_percent", "%1"},
    {"escaped", " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\x80\xFF"}};

void CheckExpansion(const string& uri_template,
                    const string& expected_expansion,
                    bool expected_validity = true,
                    const std::set<string>* expected_vars = nullptr) {
  string result;
  std::set<string> vars_found;
  EXPECT_EQ(expected_validity,
            Expand(uri_template, parameters_, &result, &vars_found));
  EXPECT_EQ(expected_expansion, result);
  if (expected_vars) {
    EXPECT_EQ(*expected_vars, vars_found);
  }
}

class UriTemplateTest : public testing::Test {};

TEST_F(UriTemplateTest, TestLevel1Templates) {
  CheckExpansion("{var}", "value");
  CheckExpansion("{hello}", "Hello%20World%21");
  CheckExpansion("{percent}", "%2531");
  CheckExpansion("{escaped}",
                 "%20%21%22%23%24%25%26%27%28%29%2A%2B%2C-.%2F%3A%3B%3C%3D%3E%"
                 "3F%40%5B%5C%5D%5E_%60%7B%7C%7D~%80%FF");
}

TEST_F(UriTemplateTest, TestLevel2Templates) {
  // Reserved string expansion
  CheckExpansion("{+var}", "value");
  CheckExpansion("{+hello}", "Hello%20World!");
  CheckExpansion("{+percent}", "%31");
  CheckExpansion("{+bad_percent}", "%251");
  CheckExpansion(
      "{+escaped}",
      "%20!%22#$%25&'()*+,-./:;%3C=%3E?@[%5C]%5E_%60%7B%7C%7D~%80%FF");
  CheckExpansion("{+path}/here", "/foo/bar/here");
  CheckExpansion("here?ref={+path}", "here?ref=/foo/bar");
  // Fragment expansion
  CheckExpansion("X{#var}", "X#value");
  CheckExpansion("X{#hello}", "X#Hello%20World!");
}

TEST_F(UriTemplateTest, TestLevel3Templates) {
  // String expansion with multiple variables
  CheckExpansion("map?{x,y}", "map?1024,768");
  CheckExpansion("{x,hello,y}", "1024,Hello%20World%21,768");
  // Reserved expansion with multiple variables
  CheckExpansion("{+x,hello,y}", "1024,Hello%20World!,768");
  CheckExpansion("{+path,x}/here", "/foo/bar,1024/here");
  // Fragment expansion with multiple variables
  CheckExpansion("{#x,hello,y}", "#1024,Hello%20World!,768");
  CheckExpansion("{#path,x}/here", "#/foo/bar,1024/here");
  // Label expansion, dot-prefixed
  CheckExpansion("X{.var}", "X.value");
  CheckExpansion("X{.x,y}", "X.1024.768");
  // Path segments, slash-prefixed
  CheckExpansion("{/var}", "/value");
  CheckExpansion("{/var,x}/here", "/value/1024/here");
  // Path-style parameters, semicolon-prefixed
  CheckExpansion("{;x,y}", ";x=1024;y=768");
  CheckExpansion("{;x,y,empty}", ";x=1024;y=768;empty");
  // Form-style query, ampersand-separated
  CheckExpansion("{?x,y}", "?x=1024&y=768");
  CheckExpansion("{?x,y,empty}", "?x=1024&y=768&empty=");
  // Form-style query continuation
  CheckExpansion("?fixed=yes{&x}", "?fixed=yes&x=1024");
  CheckExpansion("{&x,y,empty}", "&x=1024&y=768&empty=");
}

TEST_F(UriTemplateTest, TestMalformed) {
  CheckExpansion("{", "", false);
  CheckExpansion("map?{x", "", false);
  CheckExpansion("map?{x,{y}", "", false);
  CheckExpansion("map?{x,y}}", "", false);
  CheckExpansion("map?{{x,y}}", "", false);
}

TEST_F(UriTemplateTest, TestVariableSet) {
  std::set<string> expected_vars = {};
  CheckExpansion("map?{z}", "map?", true, &expected_vars);
  CheckExpansion("map{?z}", "map", true, &expected_vars);
  expected_vars = {"empty"};
  CheckExpansion("{empty}", "", true, &expected_vars);
  expected_vars = {"x", "y"};
  CheckExpansion("map?{x,y}", "map?1024,768", true, &expected_vars);
  CheckExpansion("map?{x,z,y}", "map?1024,768", true, &expected_vars);
  CheckExpansion("map{?x,z,y}", "map?x=1024&y=768", true, &expected_vars);
  expected_vars = {"y", "path"};
  CheckExpansion("{+path}{/z}{?y}&k=24", "/foo/bar?y=768&k=24", true,
                 &expected_vars);
  CheckExpansion("{y}{+path}", "768/foo/bar", true, &expected_vars);
}

}  // namespace
}  // namespace uri_template
