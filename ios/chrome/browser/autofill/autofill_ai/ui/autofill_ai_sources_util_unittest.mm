// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_sources_util.h"

#import "base/i18n/time_formatting.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
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

using GmailSourceMetadata = autofill::EntityInstance::
    PersonalContextRecordTypePayload::GmailSourceMetadata;
using PhotosSourceMetadata = autofill::EntityInstance::
    PersonalContextRecordTypePayload::PhotosSourceMetadata;
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
               Source{.url = GURL(""), .metadata = GmailSourceMetadata{}},
               Source{.url = GURL("invalid_url"),
                      .metadata = GmailSourceMetadata{}},
               Source{.url = GURL(""), .metadata = PhotosSourceMetadata{}},
           }}});

  EXPECT_FALSE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  EXPECT_EQ(groups.count, 0u);
}

// Tests extracting Gmail sources with a custom title.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_GmailWithTitle) {
  const GURL url = GURL("https://mail.google.com/mail/u/0/#inbox/msg1");
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{
                   .url = url,
                   .metadata =
                       GmailSourceMetadata{.title = "Your order confirmation"}},
           }}});

  EXPECT_TRUE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  ASSERT_EQ(groups.count, 1u);
  ASSERT_EQ(groups[0].items.count, 1u);
  EXPECT_NSEQ(groups[0].items[0].title, @"Your order confirmation");
}

// Tests extracting Gmail sources.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_GmailOnly) {
  const GURL url1("https://mail.google.com/mail/u/0/#inbox/msg1");
  const GURL url2("https://mail.google.com/mail/u/0/#inbox/msg2");
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{.url = url1, .metadata = GmailSourceMetadata{}},
               Source{.url = url2, .metadata = GmailSourceMetadata{}},
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
  EXPECT_EQ(item1.URL, url1);
  EXPECT_EQ(item1.type, AutofillAiSourceType::kGmail);
  EXPECT_NE(item1.icon, nil);

  AutofillAiSourceItem* item2 = group.items[1];
  EXPECT_NSEQ(item2.title,
              l10n_util::GetNSStringF(
                  IDS_IOS_AUTOFILL_AI_SOURCES_FALLBACK_GMAIL_MESSAGE,
                  base::NumberToString16(2)));
  EXPECT_EQ(item2.URL, url2);
  EXPECT_EQ(item2.type, AutofillAiSourceType::kGmail);
  EXPECT_NE(item2.icon, nil);
}

// Tests extracting Google Photos sources with timestamps.
TEST_F(AutofillAiSourcesUtilTest,
       TestExtractSourcesFromEntity_PhotosWithTimestamp) {
  const GURL photo_url1("https://photos.google.com/photo/123");
  const GURL photo_url2("https://photos.google.com/photo/456");
  base::Time timestamp1;
  ASSERT_TRUE(base::Time::FromUTCString("2025-10-15 14:30:00", &timestamp1));
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{
                   .url = photo_url1,
                   .metadata = PhotosSourceMetadata{.timestamp = timestamp1}},
               Source{.url = photo_url2, .metadata = PhotosSourceMetadata{}},
           }}});

  EXPECT_TRUE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  ASSERT_EQ(groups.count, 1u);

  AutofillAiSourceGroup* group = groups[0];
  EXPECT_NSEQ(
      group.title,
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_PHOTOS_SECTION_TITLE));
  ASSERT_EQ(group.items.count, 2u);

  AutofillAiSourceItem* item1 = group.items[0];
  EXPECT_NSEQ(item1.title,
              base::SysUTF16ToNSString(base::TimeFormatShortDate(timestamp1)));
  EXPECT_EQ(item1.URL, photo_url1);
  EXPECT_EQ(item1.type, AutofillAiSourceType::kPhotos);
  EXPECT_NE(item1.icon, nil);

  AutofillAiSourceItem* item2 = group.items[1];
  EXPECT_NSEQ(item2.title, l10n_util::GetNSStringF(
                               IDS_IOS_AUTOFILL_AI_SOURCES_FALLBACK_SAVED_PHOTO,
                               base::NumberToString16(1)));
  EXPECT_EQ(item2.URL, photo_url2);
  EXPECT_EQ(item2.type, AutofillAiSourceType::kPhotos);
  EXPECT_NE(item2.icon, nil);
}

// Tests extracting Google Photos sources without timestamps (fallback labels).
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_PhotosOnly) {
  const GURL photo_url1("https://photos.google.com/photo/123");
  const GURL photo_url2("https://photos.google.com/photo/456");
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{.url = photo_url1, .metadata = PhotosSourceMetadata{}},
               Source{.url = photo_url2, .metadata = PhotosSourceMetadata{}},
           }}});

  EXPECT_TRUE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  ASSERT_EQ(groups.count, 1u);

  AutofillAiSourceGroup* group = groups[0];
  EXPECT_NSEQ(
      group.title,
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_PHOTOS_SECTION_TITLE));
  ASSERT_EQ(group.items.count, 2u);

  AutofillAiSourceItem* item1 = group.items[0];
  EXPECT_NSEQ(item1.title, l10n_util::GetNSStringF(
                               IDS_IOS_AUTOFILL_AI_SOURCES_FALLBACK_SAVED_PHOTO,
                               base::NumberToString16(1)));
  EXPECT_EQ(item1.URL, photo_url1);
  EXPECT_EQ(item1.type, AutofillAiSourceType::kPhotos);
  EXPECT_NE(item1.icon, nil);

  AutofillAiSourceItem* item2 = group.items[1];
  EXPECT_NSEQ(item2.title, l10n_util::GetNSStringF(
                               IDS_IOS_AUTOFILL_AI_SOURCES_FALLBACK_SAVED_PHOTO,
                               base::NumberToString16(2)));
  EXPECT_EQ(item2.URL, photo_url2);
  EXPECT_EQ(item2.type, AutofillAiSourceType::kPhotos);
  EXPECT_NE(item2.icon, nil);
}

// Tests extracting mixed sources contains both Gmail and Photos groups in
// order.
TEST_F(AutofillAiSourcesUtilTest, TestExtractSourcesFromEntity_Mixed) {
  const GURL gmail_url("https://mail.google.com/mail/u/0/#inbox/msg1");
  const GURL photo_url("https://photos.google.com/photo/123");
  base::Time photo_timestamp;
  ASSERT_TRUE(
      base::Time::FromUTCString("2025-10-15 14:30:00", &photo_timestamp));
  autofill::EntityInstance entity = autofill::test::GetOrderEntityInstance(
      {.record_type = PersonalContextRecordTypePayload{
           .sources = {
               Source{.url = photo_url,
                      .metadata =
                          PhotosSourceMetadata{.timestamp = photo_timestamp}},
               Source{.url = gmail_url,
                      .metadata =
                          GmailSourceMetadata{.title = "Order Confirmation"}},
           }}});

  EXPECT_TRUE(EntityHasValidSources(entity));
  NSArray<AutofillAiSourceGroup*>* groups = ExtractSourcesFromEntity(entity);
  ASSERT_EQ(groups.count, 2u);

  EXPECT_NSEQ(
      groups[0].title,
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_GMAIL_SECTION_TITLE));
  ASSERT_EQ(groups[0].items.count, 1u);
  EXPECT_EQ(groups[0].items[0].URL, gmail_url);
  EXPECT_NSEQ(groups[0].items[0].title, @"Order Confirmation");

  EXPECT_NSEQ(
      groups[1].title,
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_SOURCES_PHOTOS_SECTION_TITLE));
  ASSERT_EQ(groups[1].items.count, 1u);
  EXPECT_EQ(groups[1].items[0].URL, photo_url);
  EXPECT_NSEQ(
      groups[1].items[0].title,
      base::SysUTF16ToNSString(base::TimeFormatShortDate(photo_timestamp)));
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
