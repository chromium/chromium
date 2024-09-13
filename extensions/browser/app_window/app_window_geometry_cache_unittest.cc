// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/app_window/app_window_geometry_cache.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "components/prefs/mock_pref_change_callback.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace extensions {

namespace {
const char kWindowId[] = "windowid";
const char kWindowId2[] = "windowid2";

// Create a very simple extension with id.
scoped_refptr<const Extension> CreateExtension(const ExtensionId& id) {
  return ExtensionBuilder("test").SetID(id).Build();
}

}  // namespace

// Base class for tests.
class AppWindowGeometryCacheTest : public ExtensionsTest {
 public:
  // testing::Test overrides:
  void SetUp() override;
  void TearDown() override;

  void AddGeometryAndLoadExtension(const ExtensionId& extension_id,
                                   const std::string& window_id,
                                   const gfx::Rect& bounds,
                                   const gfx::Rect& screen_bounds,
                                   ui::mojom::WindowShowState state);

  // Spins the UI threads' message loops to make sure any task
  // posted to sync the geometry to the value store gets a chance to run.
  void WaitForSync();

  void LoadExtension(const ExtensionId& extension_id);
  void UnloadExtension(const ExtensionId& extension_id);

  // Creates and adds an extension with associated prefs. Returns the extension
  // ID.
  ExtensionId AddExtensionWithPrefs(const std::string& name);

 protected:
  raw_ptr<ExtensionPrefs, DanglingUntriaged> extension_prefs_;  // Weak.
  std::unique_ptr<AppWindowGeometryCache> cache_;
};

void AppWindowGeometryCacheTest::SetUp() {
  ExtensionsTest::SetUp();
  extension_prefs_ = ExtensionPrefs::Get(browser_context());
  cache_ = std::make_unique<AppWindowGeometryCache>(browser_context(),
                                                    extension_prefs_);
  cache_->SetSyncDelayForTests(0);
}

void AppWindowGeometryCacheTest::TearDown() {
  cache_.reset();
  ExtensionsTest::TearDown();
}

void AppWindowGeometryCacheTest::AddGeometryAndLoadExtension(
    const ExtensionId& extension_id,
    const std::string& window_id,
    const gfx::Rect& bounds,
    const gfx::Rect& screen_bounds,
    ui::mojom::WindowShowState state) {
  base::Value::Dict dict;
  base::Value::Dict value;
  value.Set("x", bounds.x());
  value.Set("y", bounds.y());
  value.Set("w", bounds.width());
  value.Set("h", bounds.height());
  value.Set("screen_bounds_x", screen_bounds.x());
  value.Set("screen_bounds_y", screen_bounds.y());
  value.Set("screen_bounds_w", screen_bounds.width());
  value.Set("screen_bounds_h", screen_bounds.height());
  value.Set("state", static_cast<int>(state));
  dict.Set(window_id, std::move(value));
  extension_prefs_->SetGeometryCache(extension_id, std::move(dict));
  LoadExtension(extension_id);
}

void AppWindowGeometryCacheTest::WaitForSync() {
  content::RunAllPendingInMessageLoop();
}

void AppWindowGeometryCacheTest::LoadExtension(
    const ExtensionId& extension_id) {
  cache_->LoadGeometryFromStorage(extension_id);
  WaitForSync();
}

void AppWindowGeometryCacheTest::UnloadExtension(
    const ExtensionId& extension_id) {
  scoped_refptr<const Extension> extension = CreateExtension(extension_id);
  cache_->OnExtensionUnloaded(browser_context(), extension.get(),
                              UnloadedExtensionReason::DISABLE);
  WaitForSync();
}

ExtensionId AppWindowGeometryCacheTest::AddExtensionWithPrefs(
    const std::string& name) {
  // Generate the extension with a path based on the name so that extensions
  // with different names will have different IDs.
  base::FilePath path =
      browser_context()->GetPath().AppendASCII("Extensions").AppendASCII(name);
  scoped_refptr<const Extension> extension =
      ExtensionBuilder(name).SetPath(path).Build();

  extension_prefs_->OnExtensionInstalled(
      extension.get(),
      Extension::ENABLED,
      syncer::StringOrdinal::CreateInitialOrdinal(),
      std::string());
  return extension->id();
}

// Test getting geometry from an empty store.
TEST_F(AppWindowGeometryCacheTest, GetGeometryEmptyStore) {
  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  ASSERT_FALSE(
      cache_->GetGeometry(extension_id, kWindowId, nullptr, nullptr, nullptr));
}

// Test getting geometry for an unknown extension.
TEST_F(AppWindowGeometryCacheTest, GetGeometryUnkownExtension) {
  const ExtensionId extension_id1 = AddExtensionWithPrefs("ext1");
  const ExtensionId extension_id2 = AddExtensionWithPrefs("ext2");
  AddGeometryAndLoadExtension(extension_id1, kWindowId, gfx::Rect(4, 5, 31, 43),
                              gfx::Rect(0, 0, 1600, 900),
                              ui::mojom::WindowShowState::kNormal);
  ASSERT_FALSE(
      cache_->GetGeometry(extension_id2, kWindowId, nullptr, nullptr, nullptr));
}

// Test getting geometry for an unknown window in a known extension.
TEST_F(AppWindowGeometryCacheTest, GetGeometryUnkownWindow) {
  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  AddGeometryAndLoadExtension(extension_id, kWindowId, gfx::Rect(4, 5, 31, 43),
                              gfx::Rect(0, 0, 1600, 900),
                              ui::mojom::WindowShowState::kNormal);
  ASSERT_FALSE(
      cache_->GetGeometry(extension_id, kWindowId2, nullptr, nullptr, nullptr));
}

// Test that loading geometry, screen_bounds and state from the store works
// correctly.
TEST_F(AppWindowGeometryCacheTest, GetGeometryAndStateFromStore) {
  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  ui::mojom::WindowShowState state = ui::mojom::WindowShowState::kNormal;
  AddGeometryAndLoadExtension(
      extension_id, kWindowId, bounds, screen_bounds, state);
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::mojom::WindowShowState new_state = ui::mojom::WindowShowState::kDefault;
  ASSERT_TRUE(cache_->GetGeometry(
      extension_id, kWindowId, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_EQ(bounds, new_bounds);
  ASSERT_EQ(screen_bounds, new_screen_bounds);
  ASSERT_EQ(state, new_state);
}

// Test corrupt bounds will not be loaded.
TEST_F(AppWindowGeometryCacheTest, CorruptBounds) {
  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds;
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  ui::mojom::WindowShowState state = ui::mojom::WindowShowState::kNormal;
  AddGeometryAndLoadExtension(
      extension_id, kWindowId, bounds, screen_bounds, state);
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::mojom::WindowShowState new_state = ui::mojom::WindowShowState::kDefault;
  ASSERT_FALSE(cache_->GetGeometry(
      extension_id, kWindowId, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_TRUE(new_bounds.IsEmpty());
  ASSERT_TRUE(new_screen_bounds.IsEmpty());
  ASSERT_EQ(new_state, ui::mojom::WindowShowState::kDefault);
}

// Test corrupt screen bounds will not be loaded.
TEST_F(AppWindowGeometryCacheTest, CorruptScreenBounds) {
  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds;
  ui::mojom::WindowShowState state = ui::mojom::WindowShowState::kNormal;
  AddGeometryAndLoadExtension(
      extension_id, kWindowId, bounds, screen_bounds, state);
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::mojom::WindowShowState new_state = ui::mojom::WindowShowState::kDefault;
  ASSERT_FALSE(cache_->GetGeometry(
      extension_id, kWindowId, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_TRUE(new_bounds.IsEmpty());
  ASSERT_TRUE(new_screen_bounds.IsEmpty());
  ASSERT_EQ(new_state, ui::mojom::WindowShowState::kDefault);
}

// Test corrupt state will not be loaded.
TEST_F(AppWindowGeometryCacheTest, CorruptState) {
  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  ui::mojom::WindowShowState state = ui::mojom::WindowShowState::kDefault;
  AddGeometryAndLoadExtension(
      extension_id, kWindowId, bounds, screen_bounds, state);
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::mojom::WindowShowState new_state = ui::mojom::WindowShowState::kDefault;
  ASSERT_FALSE(cache_->GetGeometry(
      extension_id, kWindowId, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_TRUE(new_bounds.IsEmpty());
  ASSERT_TRUE(new_screen_bounds.IsEmpty());
  ASSERT_EQ(new_state, ui::mojom::WindowShowState::kDefault);
}

// Test saving geometry, screen_bounds and state to the cache and state store,
// and reading it back.
TEST_F(AppWindowGeometryCacheTest, SaveGeometryAndStateToStore) {
  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  const std::string window_id(kWindowId);

  // inform cache of extension
  LoadExtension(extension_id);

  // update geometry stored in cache
  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  ui::mojom::WindowShowState state = ui::mojom::WindowShowState::kNormal;
  cache_->SaveGeometry(extension_id, window_id, bounds, screen_bounds, state);

  // make sure that immediately reading back geometry works
  gfx::Rect new_bounds;
  gfx::Rect new_screen_bounds;
  ui::mojom::WindowShowState new_state = ui::mojom::WindowShowState::kDefault;
  ASSERT_TRUE(cache_->GetGeometry(
      extension_id, window_id, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_EQ(bounds, new_bounds);
  ASSERT_EQ(screen_bounds, new_screen_bounds);
  ASSERT_EQ(state, new_state);

  // unload extension to force cache to save data to the state store
  UnloadExtension(extension_id);

  // check if geometry got stored correctly in the state store
  const base::Value::Dict* dict =
      extension_prefs_->GetGeometryCache(extension_id);
  ASSERT_TRUE(dict);

  ASSERT_TRUE(dict->Find(window_id));

  ASSERT_EQ(bounds.x(), dict->FindIntByDottedPath(window_id + ".x"));
  ASSERT_EQ(bounds.y(), dict->FindIntByDottedPath(window_id + ".y"));
  ASSERT_EQ(bounds.width(), dict->FindIntByDottedPath(window_id + ".w"));
  ASSERT_EQ(bounds.height(), dict->FindIntByDottedPath(window_id + ".h"));
  ASSERT_EQ(screen_bounds.x(),
            dict->FindIntByDottedPath(window_id + ".screen_bounds_x"));
  ASSERT_EQ(screen_bounds.y(),
            dict->FindIntByDottedPath(window_id + ".screen_bounds_y"));
  ASSERT_EQ(screen_bounds.width(),
            dict->FindIntByDottedPath(window_id + ".screen_bounds_w"));
  ASSERT_EQ(screen_bounds.height(),
            dict->FindIntByDottedPath(window_id + ".screen_bounds_h"));
  ASSERT_EQ(static_cast<int>(state),
            dict->FindIntByDottedPath(window_id + ".state"));

  // reload extension
  LoadExtension(extension_id);
  // and make sure the geometry got reloaded properly too
  ASSERT_TRUE(cache_->GetGeometry(
      extension_id, window_id, &new_bounds, &new_screen_bounds, &new_state));
  ASSERT_EQ(bounds, new_bounds);
  ASSERT_EQ(screen_bounds, new_screen_bounds);
  ASSERT_EQ(state, new_state);
}

// Tests that we won't do writes to the state store for SaveGeometry calls
// which don't change the state we already have.
TEST_F(AppWindowGeometryCacheTest, NoDuplicateWrites) {
  using testing::_;
  using testing::Mock;

  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  gfx::Rect bounds1(100, 200, 300, 400);
  gfx::Rect bounds2(200, 400, 600, 800);
  gfx::Rect bounds2_duplicate(200, 400, 600, 800);

  gfx::Rect screen_bounds1(0, 0, 1600, 900);
  gfx::Rect screen_bounds2(0, 0, 1366, 768);
  gfx::Rect screen_bounds2_duplicate(0, 0, 1366, 768);

  MockPrefChangeCallback observer(pref_service());
  PrefChangeRegistrar registrar;
  registrar.Init(pref_service());
  registrar.Add("extensions.settings", observer.GetCallback());

  // Write the first bounds - it should do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(extension_id, kWindowId, bounds1, screen_bounds1,
                       ui::mojom::WindowShowState::kNormal);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a different bounds - it should also do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(extension_id, kWindowId, bounds2, screen_bounds1,
                       ui::mojom::WindowShowState::kNormal);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a different screen bounds - it should also do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(extension_id, kWindowId, bounds2, screen_bounds2,
                       ui::mojom::WindowShowState::kNormal);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a different state - it should also do > 0 writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_));
  cache_->SaveGeometry(extension_id, kWindowId, bounds2, screen_bounds2,
                       ui::mojom::WindowShowState::kMaximized);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);

  // Write a bounds, screen bounds and state that's a duplicate of what we
  // already have. This should not do any writes.
  EXPECT_CALL(observer, OnPreferenceChanged(_)).Times(0);
  cache_->SaveGeometry(extension_id, kWindowId, bounds2_duplicate,
                       screen_bounds2_duplicate,
                       ui::mojom::WindowShowState::kMaximized);
  WaitForSync();
  Mock::VerifyAndClearExpectations(&observer);
}

// Tests that no more than kMaxCachedWindows windows will be cached.
TEST_F(AppWindowGeometryCacheTest, MaxWindows) {
  const ExtensionId extension_id = AddExtensionWithPrefs("ext1");
  // inform cache of extension
  LoadExtension(extension_id);

  gfx::Rect bounds(4, 5, 31, 43);
  gfx::Rect screen_bounds(0, 0, 1600, 900);
  for (size_t i = 0; i < AppWindowGeometryCache::kMaxCachedWindows + 1; ++i) {
    std::string window_id = "window_" + base::NumberToString(i);
    cache_->SaveGeometry(extension_id, window_id, bounds, screen_bounds,
                         ui::mojom::WindowShowState::kNormal);
  }

  // The first added window should no longer have cached geometry.
  EXPECT_FALSE(
      cache_->GetGeometry(extension_id, "window_0", nullptr, nullptr, nullptr));
  // All other windows should still exist.
  for (size_t i = 1; i < AppWindowGeometryCache::kMaxCachedWindows + 1; ++i) {
    std::string window_id = "window_" + base::NumberToString(i);
    EXPECT_TRUE(cache_->GetGeometry(extension_id, window_id, nullptr, nullptr,
                                    nullptr));
  }
}

}  // namespace extensions
