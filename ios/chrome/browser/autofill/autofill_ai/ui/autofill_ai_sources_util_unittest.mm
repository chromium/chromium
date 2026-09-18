// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_util.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/autofill_test_util.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_source_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

using GmailSource =
    autofill::EntityInstance::PersonalContextRecordTypePayload::GmailSource;
using PhotosSource =
    autofill::EntityInstance::PersonalContextRecordTypePayload::PhotosSource;
using Source =
    autofill::EntityInstance::PersonalContextRecordTypePayload::Source;
using SourceType = Source::Type;
using PersonalContextRecordTypePayload =
    autofill::EntityInstance::PersonalContextRecordTypePayload;

using AutofillAiSourcesUtilTest = PlatformTest;

// Tests that extracting sources from an entity with no sources returns an empty
// array, and EntityHasValidSources returns false.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_EmptySources) {
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{.sources = {}}});

  EXPECT_FALSE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  EXPECT_EQ(groups.count, 0u);
}

// Tests that sources with invalid URLs are skipped.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_InvalidURLs) {
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{.url = "", .data = GmailSource{}},
               Source{.url = "invalid_url", .data = GmailSource{}},
               Source{.url = "", .data = PhotosSource{}},
           }}});

  EXPECT_FALSE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  EXPECT_EQ(groups.count, 0u);
}

// Tests extracting Gmail sources with a custom title.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_GmailWithTitle) {
  const std::string url = "https://mail.google.com/mail/u/0/#inbox/msg1";
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{.url = url,
                      .data = GmailSource{.title = "Your order confirmation"}},
           }}});

  EXPECT_TRUE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  ASSERT_EQ(groups.count, 1u);
  ASSERT_EQ(groups[0].items.count, 1u);
  EXPECT_NSEQ(groups[0].items[0].title, @"Your order confirmation");
}

// Tests extracting Gmail sources.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_GmailOnly) {
  const std::string url1 = "https://mail.google.com/mail/u/0/#inbox/msg1";
  const std::string url2 = "https://mail.google.com/mail/u/0/#inbox/msg2";
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{.url = url1, .data = GmailSource{}},
               Source{.url = url2, .data = GmailSource{}},
           }}});

  EXPECT_TRUE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  ASSERT_EQ(groups.count, 1u);

  AutofillAiSourceGroup* group = groups[0];
  EXPECT_NSEQ(
      group.title,
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_GMAIL_SECTION_TITLE));
  ASSERT_EQ(group.items.count, 2u);

  AutofillAiSourceItem* item1 = group.items[0];
  EXPECT_NSEQ(item1.title,
              l10n_util::GetNSStringF(
                  IDS_IOS_AUTOFILL_AI_SOURCES_FALLBACK_GMAIL_MESSAGE,
                  base::NumberToString16(1)));
  EXPECT_EQ(item1.URL, GURL(url1));
  EXPECT_EQ(item1.type, AutofillAiSourceType::kGmail);
  EXPECT_NE(item1.icon, nil);

  AutofillAiSourceItem* item2 = group.items[1];
  EXPECT_NSEQ(item2.title,
              l10n_util::GetNSStringF(
                  IDS_IOS_AUTOFILL_AI_SOURCES_FALLBACK_GMAIL_MESSAGE,
                  base::NumberToString16(2)));
  EXPECT_EQ(item2.URL, GURL(url2));
  EXPECT_EQ(item2.type, AutofillAiSourceType::kGmail);
  EXPECT_NE(item2.icon, nil);
}

// Tests extracting Google Photos sources.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_PhotosOnly) {
  const std::string photo_url = "https://photos.google.com/photo/123";
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{.url = photo_url, .data = PhotosSource{}},
           }}});

  EXPECT_TRUE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  ASSERT_EQ(groups.count, 1u);

  AutofillAiSourceGroup* group = groups[0];
  EXPECT_NSEQ(
      group.title,
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_PHOTOS_SECTION_TITLE));
  ASSERT_EQ(group.items.count, 1u);

  AutofillAiSourceItem* item = group.items[0];
  EXPECT_NSEQ(item.title, l10n_util::GetNSStringF(
                              IDS_IOS_AUTOFILL_AI_SOURCES_FALLBACK_SAVED_PHOTO,
                              base::NumberToString16(1)));
  EXPECT_EQ(item.URL, GURL(photo_url));
  EXPECT_EQ(item.type, AutofillAiSourceType::kPhotos);
  EXPECT_NE(item.icon, nil);
}

// Tests extracting mixed sources contains both Gmail and Photos groups in
// order.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_Mixed) {
  const std::string gmail_url = "https://mail.google.com/mail/u/0/#inbox/msg1";
  const std::string photo_url = "https://photos.google.com/photo/123";
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{.url = photo_url, .data = PhotosSource{}},
               Source{.url = gmail_url, .data = GmailSource{}},
           }}});

  EXPECT_TRUE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  ASSERT_EQ(groups.count, 2u);

  EXPECT_NSEQ(
      groups[0].title,
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_GMAIL_SECTION_TITLE));
  ASSERT_EQ(groups[0].items.count, 1u);
  EXPECT_EQ(groups[0].items[0].URL, GURL(gmail_url));

  EXPECT_NSEQ(
      groups[1].title,
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_PHOTOS_SECTION_TITLE));
  ASSERT_EQ(groups[1].items.count, 1u);
  EXPECT_EQ(groups[1].items[0].URL, GURL(photo_url));
}

// Tests that non-personal-context entities (such as local entities) return an
// empty array and false for EntityHasValidSources.
TEST_F(AutofillAiSourcesUtilTest,
       TestExtractSourcesFromEntity_NonPersonalContextEntity) {
  autofill::EntityInstance entity = autofill::test::GetPassportEntityInstance();

  EXPECT_FALSE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  EXPECT_EQ(groups.count, 0u);
}

// Tests formatting header subtitle with and without suggestion value.
TEST_F(AutofillAiSourcesUtilTest, TestSourcesHeaderSubtitle) {
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance();

  // With a suggestion value.
  NSString* subtitle_with_value = SourcesHeaderSubtitle(entity, @"AN-147338");
  EXPECT_NSEQ(subtitle_with_value, @"Order · AN-147338");

  // With empty suggestion value.
  NSString* subtitle_empty = SourcesHeaderSubtitle(entity, @"");
  EXPECT_NSEQ(subtitle_empty, @"Order");

  // With nil suggestion value.
  NSString* subtitle_nil = SourcesHeaderSubtitle(entity, nil);
  EXPECT_NSEQ(subtitle_nil, @"Order");
}

}  // namespace
