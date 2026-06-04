// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic to monitor page stability by tracking DOM mutations after
 * user interactions.
 */

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

declare global {
  interface Window {
    pageStabilityMetricsEnabled?: boolean;
  }
}

// The duration in milliseconds to monitor mutations after an interaction.
const INTERVAL_DURATION_MS = parseInt('{{INTERVAL_DURATION_MS}}', 10);

// The threshold for throttling interactions that come in quick succession.
const THROTTLE_THRESHOLD_MS = parseInt('{{THROTTLE_THRESHOLD_MS}}', 10);

// pageStabilityMetricsEnabled is injected from native code as a property
// on the window to prevent the JS compiler from doing the comparison too early.
function pageStabilityMetricsEnabled(): boolean {
  return window.pageStabilityMetricsEnabled ?? false;
}

const MUTATION_OBSERVER_OPTIONS: MutationObserverInit = {
  childList: true,
  subtree: true,
  characterData: true,
};

// The set of events that are considered user interactions for collecting
// metrics about, or waiting for, page stability.
//
// We listen to the following events since they are emitted by the web tools.
// click_tool: touchstart, touchend, mousedown, mouseup, click
// select_tool and type_tool: keydown, keypress, input, keyup, change
const INTERACTION_EVENTS = [
  'touchstart',
  'touchend',
  'mousedown',
  'mouseup',
  'click',
  'keydown',
  'keypress',
  'input',
  'keyup',
  'change',
];

// Whether event listeners for INTERACTION_EVENTS are currently attached.
let interactionListenersActive = false;

// Global counter for all DOM mutations.
let cumulativeMutationCount = 0;

// The timestamp of the last interaction that triggered a report.
let lastInteractionTime: number|null = null;

/**
 * An interval following a user interaction during which DOM mutations are
 * tracked to measure page stability.
 *
 * Note that these intervals can overlap if an earlier interaction interval is
 * still open while a later interaction causes a mutation. This is accepted
 * noise as it reflects ongoing DOM activity, but can be mitigated
 * by adjusting INTERVAL_DURATION_MS or THROTTLE_THRESHOLD_MS.
 */
interface TrackingInterval {
  /** When the interaction that triggered this interval occurred. */
  interactionTime: number;
  /** The mutation count at the time of the interaction. */
  startCount: number;
  /** The timestamp of the first mutation after the interaction. */
  firstMutationTimestamp: number|null;
  /** The timestamp of the last mutation after the interaction. */
  lastMutationTimestamp: number|null;
}

// An interface to indicate how waitForStability completed.
interface StabilityResult {
  // Whether the page settled on its own before waitForStability timed out.
  settled: boolean;
  // Whether the page didn't settle and waitForStability timed out.
  timedOut?: boolean;
  // Set if settled is true, this is the number of mutations that occurred in
  // the final tracking interval that made waitForStability resolve.
  mutationCount?: number;
}

// Interaction intervals that have not been closed yet.
const activeIntervals = new Set<TrackingInterval>();

// The observer monitoring DOM mutations.
const observer = new MutationObserver((mutations) => {
  if (activeIntervals.size === 0) {
    return;
  }

  cumulativeMutationCount += mutations.length;
  const now = performance.now();

  for (const interval of activeIntervals) {
    if (interval.firstMutationTimestamp === null) {
      interval.firstMutationTimestamp = now;
    }
    interval.lastMutationTimestamp = now;
  }
});

/** Processes user interactions and schedules a report if not throttled. */
function processUserInteraction() {
  const now = performance.now();
  // Throttle interactions that come in rapid succession.
  if (lastInteractionTime !== null &&
      (now - lastInteractionTime) < THROTTLE_THRESHOLD_MS) {
    return;
  }
  lastInteractionTime = now;
  const interactionTime = now;

  const interval: TrackingInterval = {
    interactionTime,
    startCount: cumulativeMutationCount,
    firstMutationTimestamp: null,
    lastMutationTimestamp: null,
  };

  if (activeIntervals.size === 0) {
    observer.observe(document, MUTATION_OBSERVER_OPTIONS);
  }
  activeIntervals.add(interval);

  setTimeout(() => {
    if (!activeIntervals.has(interval)) {
      return;
    }
    activeIntervals.delete(interval);
    if (activeIntervals.size === 0) {
      observer.disconnect();
    }

    const count = cumulativeMutationCount - interval.startCount;
    const timeToFirstMutation = interval.firstMutationTimestamp ?
        (interval.firstMutationTimestamp - interactionTime) :
        -1;
    const timeToLastMutation = interval.lastMutationTimestamp ?
        (interval.lastMutationTimestamp - interactionTime) :
        -1;

    sendWebKitMessage('PageStabilityMetricsHandler', {
      // These values are used in native code to emit to UMA and will have
      // max buckets selected on that side.
      mutationCount: count,
      timeToFirstMutation: timeToFirstMutation,
      timeToLastMutation: timeToLastMutation,
    });
  }, INTERVAL_DURATION_MS);
}

/** Sets up listeners to track DOM changes after certain user interactions. */
function trackMutationsAfterInteraction() {
  if (interactionListenersActive) {
    return;
  }
  interactionListenersActive = true;
  INTERACTION_EVENTS.forEach((eventName) => {
    document.addEventListener(
        eventName, processUserInteraction,
        // These event listeners do not call preventDefault() so we can set
        // passive to true.
        {passive: true});
  });
}

/** Removes interaction listeners when no longer needed. */
function removeInteractionListeners() {
  if (!interactionListenersActive) {
    return;
  }
  interactionListenersActive = false;
  INTERACTION_EVENTS.forEach((eventName) => {
    document.removeEventListener(eventName, processUserInteraction);
  });
}

/**
 * Waits for page stability by waiting for DOM mutations to fall under a
 * threshold over a given duration.
 *
 * @param options.windowDurationMs The duration of the time window, in ms,
 *     after an interaction during which DOM mutations are tracked.
 * @param options.mutationCap The maximum number of DOM mutations allowed in the
 *     time window before the page is considered unstable.
 * @param options.timeoutMs The maximum time to wait for page stability before
 *     giving up.
 *
 * @returns an object indicating whether the page is stable
 */
function waitForStability(options: {
  windowDurationMs: number,
  mutationCap: number,
  timeoutMs: number,
}): Promise<StabilityResult> {
  let isDone = false;
  let stabilityTimerId: number|null = null;
  let timeoutTimerId: number|null = null;
  let stabilityInterval: TrackingInterval|null = null;

  const cleanUp = () => {
    isDone = true;
    if (stabilityTimerId !== null) {
      clearTimeout(stabilityTimerId);
    }
    if (timeoutTimerId !== null) {
      clearTimeout(timeoutTimerId);
    }
    if (stabilityInterval !== null) {
      activeIntervals.delete(stabilityInterval);
    }
    if (activeIntervals.size === 0) {
      observer.disconnect();
      if (!pageStabilityMetricsEnabled()) {
        removeInteractionListeners();
      }
    }
  };

  const timeoutPromise = new Promise<StabilityResult>((resolve) => {
    timeoutTimerId = setTimeout(() => {
      if (isDone) {
        return;
      }
      cleanUp();
      resolve({
        settled: false,
        timedOut: true,
      });
    }, options.timeoutMs);
  });

  const stabilityPromise = new Promise<StabilityResult>((resolve) => {
    const checkStability = () => {
      if (isDone) {
        return;
      }

      stabilityInterval = {
        interactionTime: performance.now(),
        startCount: cumulativeMutationCount,
        firstMutationTimestamp: null,
        lastMutationTimestamp: null,
      };

      activeIntervals.add(stabilityInterval);
      // Ensure the observer and events are started, in case the metrics
      // gathering is disabled and hasn't done so already.
      observer.observe(document, MUTATION_OBSERVER_OPTIONS);
      trackMutationsAfterInteraction();

      stabilityTimerId = setTimeout(() => {
        if (isDone) {
          return;
        }

        const count = cumulativeMutationCount - stabilityInterval!.startCount;
        activeIntervals.delete(stabilityInterval!);
        stabilityInterval = null;

        if (count <= options.mutationCap) {
          cleanUp();
          resolve({
            settled: true,
            mutationCount: count,
          });
        } else {
          // Try for stability again in the following window.
          checkStability();
        }
      }, options.windowDurationMs);
    };

    checkStability();
  });

  return Promise.race([timeoutPromise, stabilityPromise]);
}

if (pageStabilityMetricsEnabled()) {
  observer.observe(document, MUTATION_OBSERVER_OPTIONS);
  trackMutationsAfterInteraction();
}

const pageStabilityApi = new CrWebApi('page_stability');
pageStabilityApi.addFunction('waitForStability', waitForStability);
if (!gCrWeb.hasRegisteredApi('page_stability')) {
  gCrWeb.registerApi(pageStabilityApi);
}
