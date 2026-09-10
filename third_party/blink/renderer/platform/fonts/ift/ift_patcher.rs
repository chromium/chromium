// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Exported as "third_party/blink/renderer/platform/fonts/ift_patcher_rs.h"

use incremental_font_transfer::font_patch::PatchingError;
use incremental_font_transfer::patch_group::{PatchGroup, UrlStatus};
use incremental_font_transfer::patchmap::{DesignSpace, PatchUrl, SubsetDefinition};
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
}

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
}

#[derive(Default)]
pub struct IftPatcher {
    font_data: Vec<u8>,
    patch_data: HashMap<PatchUrl, UrlStatus>,
    requested: HashSet<PatchUrl>,
}

impl From<&[u8]> for IftPatcher {
    fn from(font_data: &[u8]) -> IftPatcher {
        IftPatcher {
            font_data: font_data.to_vec(),
            patch_data: HashMap::default(),
            requested: HashSet::default(),
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

    pub fn request_patches(&mut self, subset: &IftSubsetDefinition) -> Vec<String> {
        let Ok(font) = FontRef::new(&self.font_data) else {
            return Vec::new();
        };
        let Ok(patch_group) = PatchGroup::select_next_patches(font, &self.patch_data, &subset.0)
        else {
            return Vec::new();
        };
        let mut ret = Vec::new();
        for url in patch_group.urls() {
            if self.patch_data.contains_key(url) {
                continue;
            }
            if !self.requested.insert(url.clone()) {
                continue;
            }
            // TODO(wmedrano): Consider returning an iterator over &str to avoid
            // the &str -> std::string conversion.
            ret.push(url.as_ref().to_string());
        }
        ret
    }

    pub fn add_patch_data(&mut self, patch: &str, data: &[u8]) {
        self.patch_data
            .entry(PatchUrl::new(patch))
            .or_insert_with(|| UrlStatus::Pending(data.to_vec()));
    }

    pub fn apply_patches(&mut self, subset: &IftSubsetDefinition) -> IftApplyStatus {
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
            let result = patch_group.apply_next_patches(&mut self.patch_data);
            match result {
                Ok(data) => self.font_data = data,
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
