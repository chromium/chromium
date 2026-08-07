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

/**
 * Result returned by waitForLcp.
 */
interface LcpResult {
  lcpReceived: boolean;
}

let cancelLcpCheck: (() => void)|null = null;

/**
 * Waits for the Largest Contentful Paint (LCP) event or until timeoutMs
 * expires.
 *
 * @param options.timeoutMs Maximum duration to wait for LCP.
 * @returns An object indicating whether LCP was received.
 */
function waitForLcp(options: {
  timeoutMs: number,
}): Promise<LcpResult> {
  let timeoutTimerId: number|null = null;
  let observer: PerformanceObserver|null = null;

  if (cancelLcpCheck !== null) {
    cancelLcpCheck();
  }
  const cleanUp = () => {
    cancelLcpCheck = null;
    if (timeoutTimerId !== null) {
      clearTimeout(timeoutTimerId);
    }
    if (observer !== null) {
      observer.disconnect();
    }
  };

  const cancelPromise = new Promise<LcpResult>((resolve) => {
    cancelLcpCheck = () => {
      cleanUp();
      resolve({
        lcpReceived: false,
      });
    };
  });

  const timeoutPromise = new Promise<LcpResult>((resolve) => {
    timeoutTimerId = setTimeout(() => {
      cleanUp();
      resolve({
        lcpReceived: false,
      });
    }, options.timeoutMs);
  });

  const lcpPromise = new Promise<LcpResult>((resolve) => {
    const isLcpSupported =
        typeof PerformanceObserver !== 'undefined' &&
        Boolean(
            PerformanceObserver.supportedEntryTypes?.includes(
                'largest-contentful-paint',
                ),
        );

    if (!isLcpSupported) {
      // If observing LCP is not supported in this WebKit version, don't
      // resolve and let the other promises win the race.
      return;
    }

    const entries = performance.getEntriesByType('largest-contentful-paint');
    if (entries.length > 0) {
      cleanUp();
      resolve({lcpReceived: true});
      return;
    }

    observer = new PerformanceObserver((entryList) => {
      if (entryList.getEntries().length > 0) {
        cleanUp();
        resolve({lcpReceived: true});
      }
    });
    observer.observe({type: 'largest-contentful-paint', buffered: true});
  });

  return Promise.race([cancelPromise, timeoutPromise, lcpPromise]);
}

function cancelWaitForLcp() {
  if (cancelLcpCheck !== null) {
    cancelLcpCheck();
  }
}

const pageStabilityApi = new CrWebApi('page_stability');
pageStabilityApi.addFunction('waitForStability', waitForStability);
pageStabilityApi.addFunction('cancelWaitForStability', cancelWaitForStability);
pageStabilityApi.addFunction('waitForLcp', waitForLcp);
pageStabilityApi.addFunction('cancelWaitForLcp', cancelWaitForLcp);
if (!gCrWeb.hasRegisteredApi('page_stability')) {
  gCrWeb.registerApi(pageStabilityApi);
}
