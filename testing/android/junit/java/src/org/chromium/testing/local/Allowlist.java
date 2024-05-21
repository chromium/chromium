// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * An prefix-matching allowlist implementation.
 *
 * <pre>
 * - Rules are defined in a file with one rule per-line.
 * - # Comments are allowed.
 * - Rules begin with either: "+" or "-" as a prefix (allow vs deny), and
 *   are followed by a prefix to match.
 * - Rules are evaluated in order.
 * - If no rules match, the default is to allow.
 * </pre>
 */
class Allowlist {

    private static class Rule {
        final boolean mAllow;
        final String mPrefix;

        Rule(boolean allow, String prefix) {
            mAllow = allow;
            mPrefix = prefix;
        }
    }

    private final String mFilename;
    private final List<Rule> mRules;

    private Allowlist(String filename, List<Rule> rules) {
        mFilename = filename;
        mRules = rules;
    }

    public static Allowlist allowAll() {
        return new Allowlist("<Allow-all>", Collections.emptyList());
    }

    public static Allowlist fromLines(String filename, List<String> lines) {
        ArrayList<Rule> rules = new ArrayList<>();
        for (String line : lines) {
            line = line.strip();
            char firstChar = line.isEmpty() ? '#' : line.charAt(0);
            if (firstChar == '#') {
                continue;
            }
            if (firstChar != '+' && firstChar != '-') {
                throw new RuntimeException("Expected line to start with + or -: " + line);
            }
            String prefix = line.substring(1);
            if (prefix.isEmpty()) {
                throw new RuntimeException("Found empty prefix: " + line);
            }
            rules.add(new Rule(firstChar == '+', prefix));
        }
        return new Allowlist(filename, rules);
    }

    public static Allowlist fromFile(Path path) throws IOException {
        return fromLines(path.toString(), Files.readAllLines(path));
    }

    public String getFilename() {
        return mFilename;
    }

    public boolean allow(String className) {
        for (Rule r : mRules) {
            if (className.startsWith(r.mPrefix)) {
                return r.mAllow;
            }
        }
        return true;
    }
}
