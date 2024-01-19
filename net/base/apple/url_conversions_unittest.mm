// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#import "net/base/apple/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace net {

namespace {

class URLConversionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_data_ = [NSArray
        arrayWithObjects:
            // Simple URL with protocol
            @"https://www.google.com/", @"https://www.google.com/",

            // Simple URL with protocol and query string
            @"https://www.google.com/search?q=gtest",
            @"https://www.google.com/search?q=gtest",

            // Simple URL with protocol and query string multiple params
            @"https://www.google.com/search?hl=en&q=gtest",
            @"https://www.google.com/search?hl=en&q=gtest",

            // Simple URL with protocol and query string and fragment
            @"https://www.google.com/search?q=gtest#123",
            @"https://www.google.com/search?q=gtest#123",

            // URL with ~
            @"http://www.mysite.com/~user", @"http://www.mysite.com/~user",

            // URL with #
            @"http://www.mysite.com/#123456", @"http://www.mysite.com/#123456",

            // URL with # before ?
            @"http://www.mysite.com/test#test?test",
            @"http://www.mysite.com/test#test?test",

            // URL with ? before #
            @"http://www.mysite.com/test?test#test",
            @"http://www.mysite.com/test?test#test",

            // URL with two #s
            @"http://www.mysite.com/test#test#test",
            @"http://www.mysite.com/test#test%23test",

            // URL with two ?s
            @"http://www.mysite.com/test?test?test",
            @"http://www.mysite.com/test?test?test",

            // URL with pattern ? # ?
            @"http://www.mysite.com/test?test#test?test",
            @"http://www.mysite.com/test?test#test?test",

            // URL with pattern # ? #
            @"http://www.mysite.com/test#test?test#test",
            @"http://www.mysite.com/test#test?test%23test",

            // URL with %
            @"http://www.mysite.com/%123", @"http://www.mysite.com/%123",

            // URL with [
            @"http://www.mysite.com/[123", @"http://www.mysite.com/%5B123",

            // URL with ]
            @"http://www.mysite.com/]123", @"http://www.mysite.com/%5D123",

            // URL with `
            @"http://www.mysite.com/`123", @"http://www.mysite.com/%60123",

            // URL with ^
            @"http://www.mysite.com/^123", @"http://www.mysite.com/%5E123",

            // URL with backslash (GURL canonicallizes unescaped \ to /)
            @"http://www.mysite.com/\\123", @"http://www.mysite.com//123",

            // URL with space
            @"http://www.mysite.com/~user name",
            @"http://www.mysite.com/~user%20name",

            // URL with <
            @"http://www.mysite.com/123<456",
            @"http://www.mysite.com/123%3C456",

            // URL with >
            @"http://www.mysite.com/456>123",
            @"http://www.mysite.com/456%3E123",

            // URL with |
            @"http://www.mysite.com/|123", @"http://www.mysite.com/%7C123",

            // URL with !
            @"http://www.mysite.com/!123", @"http://www.mysite.com/!123",

            // URL with ~
            @"http://www.mysite.com/~user", @"http://www.mysite.com/~user",

            // URL with & (no ?)
            @"http://www.mysite.com/&123", @"http://www.mysite.com/&123",

            // URL with '
            @"http://www.mysite.com/'user", @"http://www.mysite.com/'user",

            // URL with "
            @"http://www.mysite.com/\"user", @"http://www.mysite.com/%22user",

            // URL with (
            @"http://www.mysite.com/(123", @"http://www.mysite.com/(123",

            // URL with )
            @"http://www.mysite.com/)123", @"http://www.mysite.com/)123",

            // URL with +
            @"http://www.mysite.com/+123", @"http://www.mysite.com/+123",

            // URL with *
            @"http://www.mysite.com/*123", @"http://www.mysite.com/*123",

            // URL with space
            @"http://www.mysite.com/user name",
            @"http://www.mysite.com/user%20name",

            // URL with unescaped European accented characters
            @"http://fr.news.yahoo.com/bactérie-e-coli-ajouter-vinaigre-leau-"
             "rinçage-légumes-061425535.html",
            @"http://fr.news.yahoo.com/bact%C3%A9rie-e-coli-ajouter-vinaigre-"
             "leau-rin%C3%A7age-l%C3%A9gumes-061425535.html",

            // URL with mix of unescaped European accented characters
            @"http://fr.news.yahoo.com/bactérie-e-coli-ajouter-vinaigre-leau-"
             "rinçage-l%C3%A9gumes-061425535.html",
            @"http://fr.news.yahoo.com/bact%C3%A9rie-e-coli-ajouter-vinaigre-"
             "leau-rin%C3%A7age-l%C3%A9gumes-061425535.html",

            // URL with unescaped Asian unicode characters
            @"http://www.baidu.com/s?cl=3&fr=tb01000&wd=鍜嬩箞鍠傞",
            @"http://www.baidu.com/s?cl=3&fr=tb01000&wd="
             "%E9%8D%9C%E5%AC%A9%E7%AE%9E%E9%8D%A0%E5%82%9E",

            // URL containing every character in the range 20->7F
            @"http://google.com/ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLM"
             "NOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",
            @"http://google.com/%20!%22#$%25&'()*+,-./0123456789:;%3C=%3E?@ABCD"
             "EFGHIJKLMNOPQRSTUVWXYZ%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz"
             "%7B%7C%7D~",

            // URL containing every accented character from the range 80->FF
            @"http://google.com/¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìí"
             "îïðñòóôõö÷øùúûüýþÿ",
            @"http://google.com/%C2%BF%C3%80%C3%81%C3%82%C3%83%C3%84%C3%85%C3"
             "%86%C3%87%C3%88%C3%89%C3%8A%C3%8B%C3%8C%C3%8D%C3%8E%C3%8F%C3%90"
             "%C3%91%C3%92%C3%93%C3%94%C3%95%C3%96%C3%97%C3%98%C3%99%C3%9A%C3"
             "%9B%C3%9C%C3%9D%C3%9E%C3%9F%C3%A0%C3%A1%C3%A2%C3%A3%C3%A4%C3%A5"
             "%C3%A6%C3%A7%C3%A8%C3%A9%C3%AA%C3%AB%C3%AC%C3%AD%C3%AE%C3%AF%C3"
             "%B0%C3%B1%C3%B2%C3%B3%C3%B4%C3%B5%C3%B6%C3%B7%C3%B8%C3%B9%C3%BA"
             "%C3%BB%C3%BC%C3%BD%C3%BE%C3%BF",

            // URL containing every character in the range 20->7F repeated twice
            @"http://google.com/ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLM"
             "NOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ !\"#$%&'()*+,-"
             "./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklm"
             "nopqrstuvwxyz{|}~",
            @"http://google.com/%20!%22#$%25&'()*+,-./0123456789:;%3C=%3E?@ABCD"
             "EFGHIJKLMNOPQRSTUVWXYZ%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz"
             "%7B%7C%7D~%20!%22%23$%25&'()*+,-./0123456789:;%3C=%3E?@ABCDEFGHIJ"
             "KLMNOPQRSTUVWXYZ%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz%7B%7C"
             "%7D~",

            // Special case 7F. the weird out of place control character for DEL
            @"data:,\177", @"data:,%7F",

            // URL with some common control characters.
            // GURL simply removes most control characters.
            @"data:,\a\b\t\r\n", @"data:,",

            // All control characters but \000.
            @"data:,\001\002\003\004\005\006\007\010\011\012\013\014\015\016"
             "\017\020\021\022\023\024\025\026\027\030\031\032\033\034\035\036"
             "\037",
            @"data:,",

            // Square brackets shouldn't be escaped in IPv6 literal.
            @"http://[::1]/path/", @"http://[::1]/path/",

            // Test all possible features of a URL.
            @"http://foobar:nicate@example.com:8080/some/path/"
             "file.html;params-here?foo=bar#baz",
            @"http://foobar:nicate@example.com:8080/some/path/"
             "file.html;params-here?foo=bar#baz",

            // Test a username and password that require escaping.
            @"http://john!<]:bar[@foo.com/path",
            @"http://john!%3C%5D:bar%5B@foo.com/path",

            nil];
  }

  // NSArray of NSString pairs used for running the tests.
  // Each pair is in the form <input value, expected result>.
  NSArray<NSString*>* __strong test_data_;
};

TEST_F(URLConversionTest, TestNSURLCreationFromStrings) {
  for (NSUInteger i = 0; i < test_data_.count; i += 2) {
    NSString* input = test_data_[i];
    NSString* expected = test_data_[i + 1];
    NSURL* url = NSURLWithGURL(GURL(base::SysNSStringToUTF8(input)));
    EXPECT_NSEQ(expected, url.absoluteString);
  }
}

TEST_F(URLConversionTest, TestURLWithStringDoesNotModifyAlreadyEscapedURLs) {
  for (NSUInteger i = 0; i < test_data_.count; i += 2) {
    NSString* input = test_data_[i + 1];
    NSURL* url = NSURLWithGURL(GURL(base::SysNSStringToUTF8(input)));
    NSString* expected = test_data_[i + 1];
    // Test the expected URL is created.
    EXPECT_NSEQ(expected, url.absoluteString);
  }
}

}  // namespace

}  // namespace net
