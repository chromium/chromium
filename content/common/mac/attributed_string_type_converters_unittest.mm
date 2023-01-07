// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/common/mac/attributed_string_type_converters.h"

#include <AppKit/AppKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

class AttributedStringConverterTest : public testing::Test {
 public:
  NSMutableAttributedString* NewAttrString() {
    NSString* str = @"The quick brown fox jumped over the lazy dog.";
    return [[NSMutableAttributedString alloc] initWithString:str];
  }

  NSDictionary* FontAttribute(NSString* name, CGFloat size) {
    NSFont* font = [NSFont fontWithName:name size:size];
    return [NSDictionary dictionaryWithObject:font forKey:NSFontAttributeName];
  }

  NSAttributedString* ConvertAndRestore(NSAttributedString* str) {
    ui::mojom::AttributedStringPtr attributed_str =
        ui::mojom::AttributedString::From(str);
    return attributed_str.To<NSAttributedString*>();
  }
};

TEST_F(AttributedStringConverterTest, SimpleString) {
  base::scoped_nsobject<NSMutableAttributedString> attr_str(NewAttrString());
  [attr_str addAttributes:FontAttribute(@"Helvetica", 12.5)
                    range:NSMakeRange(0, [attr_str length])];

  NSAttributedString* ns_attributed_string = ConvertAndRestore(attr_str.get());
  EXPECT_NSEQ(attr_str.get(), ns_attributed_string);
}

TEST_F(AttributedStringConverterTest, NoAttributes) {
  base::scoped_nsobject<NSAttributedString> attr_str(NewAttrString());
  NSAttributedString* ns_attributed_string = ConvertAndRestore(attr_str.get());
  EXPECT_NSEQ(attr_str.get(), ns_attributed_string);
}

TEST_F(AttributedStringConverterTest, StripColor) {
  base::scoped_nsobject<NSMutableAttributedString> attr_str(NewAttrString());
  const NSUInteger kStringLength = [attr_str length];
  [attr_str addAttribute:NSFontAttributeName
                   value:[NSFont systemFontOfSize:26]
                   range:NSMakeRange(0, kStringLength)];
  [attr_str addAttribute:NSForegroundColorAttributeName
                   value:[NSColor redColor]
                   range:NSMakeRange(0, kStringLength)];

  NSAttributedString* ns_attributed_string = ConvertAndRestore(attr_str.get());

  NSRange range;
  NSDictionary* attrs = [ns_attributed_string attributesAtIndex:0
                                                 effectiveRange:&range];
  EXPECT_TRUE(NSEqualRanges(NSMakeRange(0, kStringLength), range));
  EXPECT_NSEQ([NSFont systemFontOfSize:26],
              [attrs objectForKey:NSFontAttributeName]);
  EXPECT_FALSE([attrs objectForKey:NSForegroundColorAttributeName]);
}

TEST_F(AttributedStringConverterTest, MultipleFonts) {
  base::scoped_nsobject<NSMutableAttributedString> attr_str(NewAttrString());
  [attr_str setAttributes:FontAttribute(@"Courier", 12)
                    range:NSMakeRange(0, 10)];
  [attr_str addAttributes:FontAttribute(@"Helvetica", 16)
                    range:NSMakeRange(12, 6)];
  [attr_str addAttributes:FontAttribute(@"Helvetica", 14)
                    range:NSMakeRange(15, 5)];

  NSAttributedString* ns_attributed_string = ConvertAndRestore(attr_str);

  EXPECT_NSEQ(attr_str.get(), ns_attributed_string);
}

TEST_F(AttributedStringConverterTest, NoPertinentAttributes) {
  base::scoped_nsobject<NSMutableAttributedString> attr_str(NewAttrString());
  [attr_str addAttribute:NSForegroundColorAttributeName
                   value:[NSColor blueColor]
                   range:NSMakeRange(0, 10)];
  [attr_str addAttribute:NSBackgroundColorAttributeName
                   value:[NSColor blueColor]
                   range:NSMakeRange(15, 5)];
  [attr_str addAttribute:NSKernAttributeName
                   value:[NSNumber numberWithFloat:2.6]
                   range:NSMakeRange(11, 3)];

  NSAttributedString* ns_attributed_string = ConvertAndRestore(attr_str.get());

  base::scoped_nsobject<NSAttributedString> expected(NewAttrString());
  EXPECT_NSEQ(expected.get(), ns_attributed_string);
}

TEST_F(AttributedStringConverterTest, NilString) {
  NSAttributedString* ns_attributed_string = ConvertAndRestore(nil);
  EXPECT_TRUE(ns_attributed_string);
  EXPECT_EQ(0U, [ns_attributed_string length]);
}

TEST_F(AttributedStringConverterTest, OutOfRange) {
  NSFont* system_font = [NSFont systemFontOfSize:10];
  std::u16string font_name = base::SysNSStringToUTF16([system_font fontName]);
  ui::mojom::AttributedStringPtr attributed_string =
      ui::mojom::AttributedString::New();
  attributed_string->string = u"Hello World";
  attributed_string->attributes.push_back(
      ui::mojom::FontAttribute::New(font_name, 12, gfx::Range(0, 5)));
  attributed_string->attributes.push_back(
      ui::mojom::FontAttribute::New(font_name, 14, gfx::Range(5, 100)));
  attributed_string->attributes.push_back(
      ui::mojom::FontAttribute::New(font_name, 16, gfx::Range(100, 5)));

  NSAttributedString* ns_attributed_string =
      attributed_string.To<NSAttributedString*>();
  EXPECT_TRUE(ns_attributed_string);

  NSRange range;
  NSDictionary* attrs = [ns_attributed_string attributesAtIndex:0
                                                 effectiveRange:&range];
  EXPECT_NSEQ([NSFont systemFontOfSize:12],
              [attrs objectForKey:NSFontAttributeName]);
  EXPECT_TRUE(NSEqualRanges(range, NSMakeRange(0, 5)));

  attrs = [ns_attributed_string attributesAtIndex:5 effectiveRange:&range];
  EXPECT_FALSE([attrs objectForKey:NSFontAttributeName]);
  EXPECT_EQ(0U, [attrs count]);
}

TEST_F(AttributedStringConverterTest, SystemFontSubstitution) {
  // Ask for a specialization of the system font that the OS will refuse to
  // instantiate via the normal font APIs.
  std::u16string font_name = u".SFNS-Regular_wdth_opsz200000_GRAD_wght2BC0000";
  ui::mojom::AttributedStringPtr attributed_string =
      ui::mojom::AttributedString::New();
  attributed_string->string = u"Hello";
  attributed_string->attributes.push_back(
      ui::mojom::FontAttribute::New(font_name, 12, gfx::Range(0, 5)));

  NSAttributedString* ns_attributed_string =
      attributed_string.To<NSAttributedString*>();
  EXPECT_TRUE(ns_attributed_string);

  NSRange range;
  NSDictionary* attrs = [ns_attributed_string attributesAtIndex:0
                                                 effectiveRange:&range];
  EXPECT_NSEQ([NSFont systemFontOfSize:12],
              [attrs objectForKey:NSFontAttributeName]);
  EXPECT_TRUE(NSEqualRanges(range, NSMakeRange(0, 5)));
}
