// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maximum number of longest interactions to keep in memory for INP.
// A cap of 10 matches Google's official web-vitals.js library and yields the
// exact 98th percentile for up to 500 interactions per page.
const MAX_INTERACTIONS_TO_KEEP = 10;

// Type aliases for interaction identifiers and duration units.
type InteractionId = number;
type DurationMs = number;

// Manager to help track the longest user interaction durations in a frame.
export class InteractionManager {
  // Map to track the maximum event duration (ms) per interaction ID.
  private readonly longestDurationsByID = new Map<InteractionId, DurationMs>();

  // Total number of unique user interactions observed in this frame.
  private interactionCount = 0;

  // Cached ID and minimum duration among the tracked entries.
  // Allows O(1) instant rejection of fast interactions and O(1)
  // eviction once the map is full.
  private minEntry: {id: InteractionId, duration: DurationMs}|null = null;

  // Total number of unique user interactions observed in this frame.
  get totalCount(): number {
    return this.interactionCount;
  }

  // Records or updates a user interaction duration.
  record(eventId: InteractionId, duration: DurationMs): void {
    // An event ID is reused by WebKit for events that are part of the same
    // interaction (pointer down, pointer up, click, etc.).
    const existingDuration = this.longestDurationsByID.get(eventId);
    if (existingDuration !== undefined) {
      // For INP, the latency of an interaction is the maximum duration among
      // all events belonging to the same interactionId.
      if (duration > existingDuration) {
        this.longestDurationsByID.set(eventId, duration);
        // If the minimum entry's duration increased, re-scan to find the new
        // minimum.
        if (this.minEntry && eventId === this.minEntry.id) {
          this.updateMinEntry();
        }
      }
      return;
    }

    this.interactionCount++;

    // We only need to keep the MAX_INTERACTIONS_TO_KEEP longest interactions.
    if (this.longestDurationsByID.size < MAX_INTERACTIONS_TO_KEEP) {
      this.longestDurationsByID.set(eventId, duration);
      if (this.longestDurationsByID.size === MAX_INTERACTIONS_TO_KEEP) {
        this.updateMinEntry();
      }
    } else if (this.minEntry && duration > this.minEntry.duration) {
      // Evict the shortest duration interaction in the map since the new
      // interaction has a longer duration which pushes out the minEntry from
      // the map.
      this.longestDurationsByID.delete(this.minEntry.id);
      this.longestDurationsByID.set(eventId, duration);

      // Re-scan once to find the new minimum for the next cycle.
      this.updateMinEntry();
    }
  }

  // Scans the map once to update the cached minimum interaction ID and
  // duration.
  private updateMinEntry(): void {
    let min: {id: InteractionId, duration: DurationMs}|null = null;
    for (const [id, duration] of this.longestDurationsByID) {
      if (!min || duration < min.duration) {
        min = {id, duration};
      }
    }
    this.minEntry = min;
  }

  // Returns the tracked longest interaction durations.
  getLongestDurations(): DurationMs[] {
    return Array.from(this.longestDurationsByID.values());
  }

  // Clears all tracked state.
  clear(): void {
    this.longestDurationsByID.clear();
    this.minEntry = null;
    this.interactionCount = 0;
  }
}
