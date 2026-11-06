// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Exported as "third_party/blink/renderer/platform/fonts/ift_patcher_rs.h"

use incremental_font_transfer::font_patch::PatchingError;
use incremental_font_transfer::patch_group::{PatchGroup, UrlStatus};
use incremental_font_transfer::patchmap::{
    DesignSpace, FeatureSet, PatchMap, PatchUrl, SubsetDefinition,
};
use log::warn;
use read_fonts::types::{Fixed, Tag};
use read_fonts::{FontRef, TableProvider};
use std::collections::{HashMap, HashSet};

/// Status code returned when applying patches to an Incremental Font
/// Transfer (IFT) patcher.
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
#[repr(C)]
pub enum IftApplyStatus {
    /// All available patches were applied successfully.
    Success,
    /// The font data could not be parsed as a valid OpenType font.
    InvalidFont,
    /// Selecting the next patch group failed.
    PatchGroupError,
    /// Applying the patch payload failed.
    PatchError,
    /// Required patch data has not been provided.
    MissingPatches,
    /// More invalidating patches were needed than the IFT spec allows in a
    /// single extension.
    PatchLimitExceeded,
}

// Limits on the patches loaded and applied during a single extension. See step
// 9 of https://www.w3.org/TR/IFT/#extend-font-subset.
const MAX_INVALIDATING_PATCHES: u32 = 100;
const MAX_TOTAL_PATCHES: u32 = 2000;

#[derive(Clone, Default)]
pub struct IftSubsetDefinition(SubsetDefinition);

impl IftSubsetDefinition {
    pub fn add_codepoint(&mut self, codepoint: u32) -> bool {
        self.0.codepoints.insert(codepoint)
    }

    pub fn add_feature_tag(&mut self, tag: u32) -> bool {
        self.0.feature_tags.insert(Tag::from_u32(tag))
    }

    pub fn add_design_space(&mut self, tag: u32, min_val: f64, max_val: f64) -> bool {
        let tag = Tag::from_u32(tag);
        match &mut self.0.design_space {
            DesignSpace::Ranges(ranges) => {
                let start = Fixed::from_f64(min_val);
                let end = Fixed::from_f64(max_val);
                let entry = ranges.entry(tag).or_default();
                let prev = entry.clone();
                entry.insert(start..=end);
                *entry != prev
            }
            DesignSpace::All => false,
        }
    }

    pub fn union(&mut self, other: &IftSubsetDefinition) {
        self.0.union(&other.0);
    }

    pub fn clear(&mut self) {
        self.0.codepoints.clear();
        match &mut self.0.feature_tags {
            FeatureSet::Set(set) => set.clear(),
            FeatureSet::All => self.0.feature_tags = FeatureSet::default(),
        }
        match &mut self.0.design_space {
            DesignSpace::Ranges(ranges) => ranges.clear(),
            DesignSpace::All => self.0.design_space = DesignSpace::default(),
        }
    }
}

#[derive(Clone, Default)]
pub struct IftPatcher {
    font_data: Vec<u8>,
    /// The patch map of the font data as of the last `mark_rendered()` call, or
    /// of the initial font data if it was never called.
    rendered_patch_map: PatchMap,
    /// Whether `font_data` was modified since the last `mark_rendered()` call.
    has_unrendered_changes: bool,
    patch_data: HashMap<PatchUrl, UrlStatus>,
    requested: HashSet<PatchUrl>,
    /// The max number of Partial or Full Invalidation patches applied per
    /// extension.
    max_invalidating_patches: u32,
    /// The max number of patches of any type requested per extension.
    max_total_patches: u32,
    /// The number of patches requested during the current extension.
    requested_patch_count: u32,
    /// The number of invalidating patches applied during the current extension.
    applied_invalidating_patch_count: u32,
    /// Whether a limit was exceeded during the current extension. Cleared by
    /// `begin_extension()`.
    patch_limit_exceeded: bool,
}

fn create_patch_map(font_data: &[u8]) -> PatchMap {
    let Ok(font) = FontRef::new(font_data).inspect_err(|err| warn!("Invalid font: {err}")) else {
        return PatchMap::default();
    };
    PatchMap::new(&font).inspect_err(|err| warn!("Invalid IFT font: {err}")).unwrap_or_default()
}

impl From<&[u8]> for IftPatcher {
    fn from(font_data: &[u8]) -> IftPatcher {
        IftPatcher {
            font_data: font_data.to_vec(),
            rendered_patch_map: create_patch_map(font_data),
            has_unrendered_changes: false,
            patch_data: HashMap::default(),
            requested: HashSet::default(),
            max_invalidating_patches: MAX_INVALIDATING_PATCHES,
            max_total_patches: MAX_TOTAL_PATCHES,
            requested_patch_count: 0,
            applied_invalidating_patch_count: 0,
            patch_limit_exceeded: false,
        }
    }
}

impl IftPatcher {
    pub fn is_ift(font_data: &[u8]) -> bool {
        let Ok(font) = FontRef::new(font_data) else {
            return false;
        };
        font.ift().is_ok() || font.iftx().is_ok()
    }

    pub fn font_data(&self) -> &[u8] {
        &self.font_data
    }

    pub fn rendered_font_requires_patches(&self, subset: &IftSubsetDefinition) -> bool {
        self.rendered_patch_map.has_intersecting_patches(&subset.0)
    }

    pub fn has_unrendered_changes(&self) -> bool {
        self.has_unrendered_changes
    }

    pub fn mark_rendered(&mut self) {
        if self.has_unrendered_changes {
            self.rendered_patch_map = create_patch_map(&self.font_data);
            self.has_unrendered_changes = false;
        }
    }

    pub fn begin_extension(&mut self) {
        self.requested_patch_count = 0;
        self.applied_invalidating_patch_count = 0;
        self.patch_limit_exceeded = false;
    }

    pub fn set_patch_limits(&mut self, max_invalidating: u32, max_total: u32) {
        self.max_invalidating_patches = max_invalidating;
        self.max_total_patches = max_total;
    }

    pub fn patch_limit_exceeded(&self) -> bool {
        self.patch_limit_exceeded
    }

    pub fn request_patches(&mut self, subset: &IftSubsetDefinition) -> Vec<String> {
        if self.patch_limit_exceeded {
            return Vec::new();
        }
        let Ok(font) = FontRef::new(&self.font_data) else {
            return Vec::new();
        };
        let Ok(patch_group) = PatchGroup::select_next_patches(font, &self.patch_data, &subset.0)
        else {
            return Vec::new();
        };
        let mut seen = HashSet::new();
        let new_urls: Vec<&PatchUrl> = patch_group
            .urls()
            .filter(|url| !self.patch_data.contains_key(*url) && !self.requested.contains(*url))
            .filter(|url| seen.insert(*url))
            .collect();
        let Some(requested) = (self.requested_patch_count as usize)
            .checked_add(new_urls.len())
            .filter(|&requested| requested <= self.max_total_patches as usize)
        else {
            warn!(
                "IFT patch limit exceeded: {} + {} patches requested (limit: {})",
                self.requested_patch_count,
                new_urls.len(),
                self.max_total_patches
            );
            self.patch_limit_exceeded = true;
            return Vec::new();
        };
        self.requested_patch_count = requested as u32;
        // TODO(wmedrano): Consider returning an iterator over &str to avoid
        // the &str -> std::string conversion.
        new_urls
            .into_iter()
            .map(|url| {
                self.requested.insert(url.clone());
                url.as_ref().to_string()
            })
            .collect()
    }

    pub fn add_patch_data(&mut self, patch: &str, data: &[u8]) {
        self.patch_data
            .entry(PatchUrl::new(patch))
            .or_insert_with(|| UrlStatus::Pending(data.to_vec()));
    }

    pub fn apply_patches(&mut self, subset: &IftSubsetDefinition) -> IftApplyStatus {
        if self.patch_limit_exceeded {
            return IftApplyStatus::PatchLimitExceeded;
        }
        loop {
            let Ok(font) = FontRef::new(&self.font_data) else {
                return IftApplyStatus::InvalidFont;
            };
            let Ok(patch_group) =
                PatchGroup::select_next_patches(font, &self.patch_data, &subset.0)
                    .inspect_err(|err| warn!("IFT error: {err}"))
            else {
                return IftApplyStatus::PatchGroupError;
            };
            // Mirrors `apply_next_patches()`, which applies the next
            // invalidating patch only if its data is pending.
            let applies_invalidating = patch_group.next_invalidating_patch().is_some_and(|patch| {
                matches!(
                    self.patch_data.get(&PatchUrl::new(patch.url())),
                    Some(UrlStatus::Pending(_))
                )
            });
            if applies_invalidating
                && self.applied_invalidating_patch_count >= self.max_invalidating_patches
            {
                warn!("IFT invalidating patch limit exceeded");
                self.patch_limit_exceeded = true;
                return IftApplyStatus::PatchLimitExceeded;
            }
            let result = patch_group.apply_next_patches(&mut self.patch_data);
            match result {
                Ok(data) => {
                    self.font_data = data;
                    self.has_unrendered_changes = true;
                    if applies_invalidating {
                        self.applied_invalidating_patch_count =
                            self.applied_invalidating_patch_count.saturating_add(1);
                    }
                }
                Err(PatchingError::EmptyPatchList) => return IftApplyStatus::Success,
                Err(PatchingError::MissingPatches) => return IftApplyStatus::MissingPatches,
                Err(err) => {
                    warn!("IFT error: {err}");
                    return IftApplyStatus::PatchError;
                }
            };
        }
    }

    pub fn has_pending_patch_requests(&self) -> bool {
        self.requested.iter().any(|url| !self.patch_data.contains_key(url))
    }
}
