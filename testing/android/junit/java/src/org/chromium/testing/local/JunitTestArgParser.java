// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.testing.local;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Parses command line arguments for JunitTestMain. */
public class JunitTestArgParser {
    final Set<String> mPackageFilters = new HashSet<>();
    final Set<Class<?>> mRunnerFilters = new HashSet<>();
    final Set<String> mGtestFilters = new HashSet<>();
    Allowlist mShadowsAllowlist = Allowlist.allowAll();
    boolean mListTests;
    String mJsonConfig;
    String mJsonOutput;

    public static JunitTestArgParser parse(String[] args) {
        JunitTestArgParser parsed = new JunitTestArgParser();

        for (int i = 0; i < args.length; ++i) {
            if (args[i].startsWith("--")) {
                String argName = args[i].substring(2);
                try {
                    if ("list-tests".equals(argName)) {
                        parsed.mListTests = true;
                    } else if ("package-filter".equals(argName)) {
                        parsed.mPackageFilters.add(args[++i]);
                    } else if ("runner-filter".equals(argName)) {
                        parsed.mRunnerFilters.add(Class.forName(args[++i]));
                    } else if ("gtest-filter".equals(argName)) {
                        parsed.mGtestFilters.add(args[++i]);
                    } else if ("test-launcher-filter-file".equals(argName)) {
                        parsed.mGtestFilters.add(readFilterFile(args[++i]));
                    } else if ("json-results".equals(argName)) {
                        parsed.mJsonOutput = args[++i];
                    } else if ("json-config".equals(argName)) {
                        parsed.mJsonConfig = args[++i];
                    } else if ("shadows-allowlist".equals(argName)) {
                        parsed.mShadowsAllowlist = Allowlist.fromFile(Path.of(args[++i]));
                    } else {
                        System.out.println("Ignoring flag: \"" + argName + "\"");
                    }
                } catch (ArrayIndexOutOfBoundsException e) {
                    System.err.println("No value specified for argument \"" + argName + "\"");
                    System.exit(1);
                } catch (ClassNotFoundException e) {
                    System.err.println("Class not found. (" + e + ")");
                    System.exit(1);
                } catch (IOException e) {
                    e.printStackTrace();
                    System.exit(1);
                }
            } else {
                System.out.println("Ignoring argument: \"" + args[i] + "\"");
            }
        }
        if (parsed.mJsonConfig == null) {
            System.err.println("Missing required argument --json-config.");
            System.exit(1);
        }

        return parsed;
    }

    private static String readFilterFile(String path) throws IOException {
        List<String> lines = Files.readAllLines(Path.of(path));
        List<String> positive = new ArrayList<>();
        List<String> negative = new ArrayList<>();
        for (String line : lines) {
            int commentIdx = line.indexOf('#');
            if (commentIdx != -1) {
                line = line.substring(0, commentIdx);
            }
            line = line.trim();
            if (line.isEmpty()) {
                continue;
            }
            if (line.startsWith("-")) {
                negative.add(line.substring(1).replace('#', '.'));
            } else {
                positive.add(line.replace('#', '.'));
            }
        }
        StringBuilder sb = new StringBuilder();
        sb.append(String.join(":", positive));
        if (!negative.isEmpty()) {
            sb.append("-");
            sb.append(String.join(":", negative));
        }
        return sb.toString();
    }
}
