// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logic to wait for page stability by tracking DOM mutations.
 */

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
const MUTATION_OBSERVER_OPTIONS: MutationObserverInit = {
  childList: true,
  subtree: true,
  characterData: true,
};

// Global counter for all DOM mutations.
let cumulativeMutationCount = 0;

interface TrackingInterval {
  /** The mutation count at the start of the observation interval. */
  startCount: number;
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

// Tracking intervals that have not been closed yet.
const activeIntervals = new Set<TrackingInterval>();

// A function to cancel an earlier call to `waitForStability`.
let cancelStabilityCheck: (() => void)|null = null;


// The observer monitoring DOM mutations.
const observer = new MutationObserver((mutations) => {
  if (activeIntervals.size === 0) {
    return;
  }

  cumulativeMutationCount += mutations.length;
});



/**
 * Waits for page stability by waiting for DOM mutations to fall under a
 * threshold over a given duration.
 *
 * @param options.windowDurationMs The duration of the time window, in ms,
 *     during which DOM mutations are tracked.
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
  if (cancelStabilityCheck !== null) {
    cancelStabilityCheck();
  }

  let stabilityTimerId: number|null = null;
  let timeoutTimerId: number|null = null;
  let stabilityInterval: TrackingInterval|null = null;

  const cleanUp = () => {
    cancelStabilityCheck = null;
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
    }
  };

  const cancelPromise = new Promise<StabilityResult>((resolve) => {
    cancelStabilityCheck = () => {
      cleanUp();
      resolve({
        settled: false,
      });
    };
  });

  const timeoutPromise = new Promise<StabilityResult>((resolve) => {
    timeoutTimerId = setTimeout(() => {
      cleanUp();
      resolve({
        settled: false,
        timedOut: true,
      });
    }, options.timeoutMs);
  });

  const stabilityPromise = new Promise<StabilityResult>((resolve) => {
    const checkStability = () => {
      stabilityInterval = {
        startCount: cumulativeMutationCount,
      };

      activeIntervals.add(stabilityInterval);
      observer.observe(document, MUTATION_OBSERVER_OPTIONS);

      stabilityTimerId = setTimeout(() => {
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

  return Promise.race([cancelPromise, timeoutPromise, stabilityPromise]);
}

function cancelWaitForStability() {
  if (cancelStabilityCheck !== null) {
    cancelStabilityCheck();
  }
}


const pageStabilityApi = new CrWebApi('page_stability');
pageStabilityApi.addFunction('waitForStability', waitForStability);
pageStabilityApi.addFunction('cancelWaitForStability', cancelWaitForStability);
if (!gCrWeb.hasRegisteredApi('page_stability')) {
  gCrWeb.registerApi(pageStabilityApi);
}
