// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/hit_region.h"

#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"

namespace blink {

HitRegion::HitRegion(const Path& path, const HitRegionOptions* options)
    : id_(options->id().IsEmpty() ? String() : options->id()),
      control_(options->control()),
      path_(path) {
  if (options->fillRule() != "evenodd")
    fill_rule_ = RULE_NONZERO;
  else
    fill_rule_ = RULE_EVENODD;
}

bool HitRegion::Contains(const FloatPoint& point) const {
  return path_.Contains(point, fill_rule_);
}

void HitRegion::RemovePixels(const Path& clear_area) {
  path_.SubtractPath(clear_area);
}

void HitRegion::Trace(Visitor* visitor) const {
  visitor->Trace(control_);
}

void HitRegionManager::AddHitRegion(HitRegion* hit_region) {
  hit_region_list_.insert(hit_region);

  if (!hit_region->Id().IsEmpty())
    hit_region_id_map_.Set(hit_region->Id(), hit_region);

  if (hit_region->Control())
    hit_region_control_map_.Set(hit_region->Control(), hit_region);
}

void HitRegionManager::RemoveHitRegion(HitRegion* hit_region) {
  if (!hit_region)
    return;

  if (!hit_region->Id().IsEmpty())
    hit_region_id_map_.erase(hit_region->Id());

  if (hit_region->Control())
    hit_region_control_map_.erase(hit_region->Control());

  hit_region_list_.erase(hit_region);
}

void HitRegionManager::RemoveHitRegionById(const String& id) {
  if (!id.IsEmpty())
    RemoveHitRegion(GetHitRegionById(id));
}

void HitRegionManager::RemoveHitRegionByControl(const Element* control) {
  RemoveHitRegion(GetHitRegionByControl(control));
}

void HitRegionManager::RemoveHitRegionsInRect(const FloatRect& rect,
                                              const AffineTransform& ctm) {
  Path clear_area;
  clear_area.AddRect(rect);
  clear_area.Transform(ctm);

  HitRegionIterator it_end = hit_region_list_.rend();
  HitRegionList to_be_removed;

  for (HitRegionIterator it = hit_region_list_.rbegin(); it != it_end; ++it) {
    HitRegion* hit_region = *it;
    hit_region->RemovePixels(clear_area);
    if (hit_region->GetPath().IsEmpty())
      to_be_removed.insert(hit_region);
  }

  it_end = to_be_removed.rend();
  for (HitRegionIterator it = to_be_removed.rbegin(); it != it_end; ++it)
    RemoveHitRegion(it->Get());
}

void HitRegionManager::RemoveAllHitRegions() {
  hit_region_list_.clear();
  hit_region_id_map_.clear();
  hit_region_control_map_.clear();
}

HitRegion* HitRegionManager::GetHitRegionById(const String& id) const {
  // TODO(https://crbug.com/1236734) Refactor call to deprecated method.
  return hit_region_id_map_.DeprecatedAtOrEmptyValue(id);
}

HitRegion* HitRegionManager::GetHitRegionByControl(
    const Element* control) const {
  // TODO(https://crbug.com/1236734) Refactor call to deprecated method.
  if (control)
    return hit_region_control_map_.DeprecatedAtOrEmptyValue(control);

  return nullptr;
}

HitRegion* HitRegionManager::GetHitRegionAtPoint(
    const FloatPoint& point) const {
  HitRegionIterator it_end = hit_region_list_.rend();

  for (HitRegionIterator it = hit_region_list_.rbegin(); it != it_end; ++it) {
    HitRegion* hit_region = *it;
    if (hit_region->Contains(point))
      return hit_region;
  }

  return nullptr;
}

unsigned HitRegionManager::GetHitRegionsCount() const {
  return hit_region_list_.size();
}

void HitRegionManager::Trace(Visitor* visitor) const {
  visitor->Trace(hit_region_list_);
  visitor->Trace(hit_region_id_map_);
  visitor->Trace(hit_region_control_map_);
}

}  // namespace blink
