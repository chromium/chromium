// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Recursively inspects a list of CSS rules to find
 * `@media (prefers-color-scheme: dark)`.
 */
function hasDarkRules(rules: CSSRuleList): boolean {
  for (let i = 0; i < rules.length; i++) {
    const rule = rules[i];
    if (!rule) continue;

    // Check if it is a media rule and contains prefers-color-scheme: dark
    if (rule instanceof CSSMediaRule) {
      const mediaRule = rule as CSSMediaRule;
      if (mediaRule.media.mediaText.includes('prefers-color-scheme: dark')) {
        return true;
      }
    }

    // Recursively check nested rules (like within @supports or nested @media)
    if ('cssRules' in rule) {
      const nestedRules = (rule as any).cssRules as CSSRuleList;
      if (nestedRules && hasDarkRules(nestedRules)) {
        return true;
      }
    }
  }
  return false;
}

interface DarkModeSupport {
  supportsViaMeta: boolean;
  supportsViaCss: boolean;
  supportsViaMediaQuery: boolean;
}

/**
 * Detects if the current page natively supports dark mode.
 */
function detectDarkModeSupport(): DarkModeSupport {
  let supportsViaMeta = false;
  let supportsViaCss = false;
  let supportsViaMediaQuery = false;

  // 1. Check HTML <meta name="color-scheme">
  const meta = document.querySelector('meta[name="color-scheme"]');
  if (meta) {
    const content = meta.getAttribute('content')?.toLowerCase() || '';
    if (content.includes('dark')) {
      supportsViaMeta = true;
    }
  }

  // 2. Check computed color-scheme style of the root element
  const rootColorScheme =
      getComputedStyle(document.documentElement).colorScheme;
  if (rootColorScheme && rootColorScheme.toLowerCase().includes('dark')) {
    supportsViaCss = true;
  }

  // 3. Inspect document stylesheets for `@media (prefers-color-scheme: dark)`
  try {
    for (const sheet of document.styleSheets) {
      try {
        const rules = sheet.cssRules || sheet.rules;
        if (!rules) continue;
        if (hasDarkRules(rules)) {
          supportsViaMediaQuery = true;
          break;
        }
      } catch (e) {
        // Accessing cssRules can throw a SecurityError for cross-origin
        // stylesheets (CORS). We gracefully catch and skip to the next sheet.
      }
    }
  } catch (e) {
    // Gracefully handle outer stylesheet access errors.
  }

  return {
    supportsViaMeta,
    supportsViaCss,
    supportsViaMediaQuery,
  };
}

// Re-evaluate and report only when all resources (stylesheets, images, etc.)
// are fully loaded, guaranteeing the metrics are reported exactly once.
const reportSupport = () => {
  const support = detectDarkModeSupport();
  sendWebKitMessage('DarkModeDetectionMessageHandler', {
    'supportsViaMeta': support.supportsViaMeta,
    'supportsViaCss': support.supportsViaCss,
    'supportsViaMediaQuery': support.supportsViaMediaQuery,
  });
};

if (document.readyState === 'complete') {
  reportSupport();
} else {
  window.addEventListener('load', reportSupport);
}
