// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import org.junit.runner.Description;
import org.junit.runner.manipulation.Filter;

import java.util.HashSet;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Filters tests based on a googletest-style filter string. */
class GtestFilter extends Filter {

    private final String mFilterString;

    private final Set<Pattern> mPositiveRegexes;
    private final Set<Pattern> mNegativeRegexes;

    private static final Pattern ASTERISK = Pattern.compile("\\*");
    private static final Pattern COLON = Pattern.compile(":");
    private static final Pattern DASH = Pattern.compile("-");
    private static final Pattern DOLLAR = Pattern.compile("\\$");
    private static final Pattern PERIOD = Pattern.compile("\\.");
    private static final Pattern OPEN_BRACKET = Pattern.compile("\\[");
    private static final Pattern CLOSED_BRACKET = Pattern.compile("\\]");

    // Matches a test that can have an SDK version in the name: org.class.testInvalidMinidump[28]
    private static final Pattern GTEST_NAME_REGEX = Pattern.compile("(.*)?\\[\\d+\\]$");

    /**
     *  Creates the filter and converts the provided googletest-style filter
     *  string into positive and negative regexes.
     */
    public GtestFilter(String filterString) {
        mFilterString = filterString;
        String[] filterStrings = DASH.split(filterString, 2);
        mPositiveRegexes = generatePatternSet(filterStrings[0]);
        if (filterStrings.length == 2) {
            mNegativeRegexes = generatePatternSet(filterStrings[1]);
        } else {
            mNegativeRegexes = new HashSet<Pattern>();
        }
    }

    private Set<Pattern> generatePatternSet(String filterString) {
        Set<Pattern> patterns = new HashSet<Pattern>();
        String[] filterStrings = COLON.split(filterString);
        for (String f : filterStrings) {
            if (f.isEmpty()) continue;

            String sanitized = PERIOD.matcher(f).replaceAll(Matcher.quoteReplacement("\\."));
            sanitized = DOLLAR.matcher(sanitized).replaceAll(Matcher.quoteReplacement("\\$"));
            sanitized = ASTERISK.matcher(sanitized).replaceAll(".*");
            sanitized = OPEN_BRACKET.matcher(sanitized).replaceAll(Matcher.quoteReplacement("\\["));
            sanitized =
                    CLOSED_BRACKET.matcher(sanitized).replaceAll(Matcher.quoteReplacement("\\]"));
            patterns.add(Pattern.compile(sanitized));
        }
        return patterns;
    }

    /**
     *  Determines whether or not a test with the provided description should
     *  run based on the configured positive and negative regexes.
     *
     *  A test should run if:
     *    - it's just a class, OR
     *    - it doesn't match any of the negative regexes, AND
     *    - either:
     *      - there are no configured positive regexes, OR
     *      - it matches at least one of the positive regexes.
     */
    @Override
    public boolean shouldRun(Description description) {
        if (description.getMethodName() == null) return true;

        String gtestSdkName = description.getClassName() + "." + description.getMethodName();
        String gtestName = description.getClassName() + "." + description.getMethodName();
        // Use regex to get test name without sdk appended to make filtering more intuitive.
        // Some tests may not have an sdk version.
        Matcher gtestNameMatcher = GTEST_NAME_REGEX.matcher(gtestName);
        if (gtestNameMatcher.find()) {
            gtestName = gtestNameMatcher.group(1);
        }

        for (Pattern p : mNegativeRegexes) {
            if (p.matcher(gtestName).matches() || p.matcher(gtestSdkName).matches()) return false;
        }

        if (mPositiveRegexes.isEmpty()) return true;

        for (Pattern p : mPositiveRegexes) {
            if (p.matcher(gtestName).matches() || p.matcher(gtestSdkName).matches()) return true;
        }

        return false;
    }

    /** Returns a description of this filter. */
    @Override
    public String describe() {
        return "gtest-filter: " + mFilterString;
    }
}
