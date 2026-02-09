// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cpu_performance/cpu_performance.h"

#include <atomic>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/re2/src/re2/re2.h"

// The implementation of the CPU Performance API is experimental and subject to
// change.

namespace content::cpu_performance {

namespace {

bool Search(std::string_view text, const re2::RE2& re) {
  return re2::RE2::PartialMatch(text, re);
}

void Replace(std::string* text,
             const re2::RE2& re,
             std::string_view replacement) {
  re2::RE2::GlobalReplace(text, re, replacement);
}

void ReplaceFirst(std::string* text,
                  const re2::RE2& re,
                  std::string_view replacement) {
  re2::RE2::Replace(text, re, replacement);
}

void TrimAndCollapseWhitespace(std::string* text) {
  // \p{Z} matches unicode separators (including NBSP).
  // \x1C\x1F match the file and unit separator characters.
  Replace(text, "^[\\s\\p{Z}\\x1C\\x1F]+", "");
  Replace(text, "[\\s\\p{Z}\\x1C\\x1F]+$", "");
  Replace(text, "[\\s\\p{Z}\\x1C\\x1F]+", " ");
}

}  // anonymous namespace

// Returns the manufacturer.
Manufacturer GetManufacturer(std::string_view cpu_model) {
  if (Search(cpu_model, "(?i)\\bAMD\\b")) {
    return Manufacturer::kAMD;
  } else if (Search(cpu_model, "(?i)\\bApple\\b")) {
    return Manufacturer::kApple;
  } else if (Search(cpu_model, "(?i)\\b(Intel|Celeron|Pentium)\\b")) {
    return Manufacturer::kIntel;
  } else if (Search(cpu_model, "(?i)\\bMediaTek\\b")) {
    return Manufacturer::kMediaTek;
  } else if (Search(cpu_model, "(?i)\\bMicrosoft\\b")) {
    return Manufacturer::kMicrosoft;
  } else if (Search(cpu_model, "(?i)\\b(Qualcomm|Snapdragon)\\b")) {
    return Manufacturer::kQualcomm;
  } else if (Search(cpu_model, "(?i)\\bSamsung\\b")) {
    return Manufacturer::kSamsung;
  }
  return Manufacturer::kUnknown;
}

std::pair<Manufacturer, std::string> SplitCpuModel(std::string_view cpu_model) {
  std::string text(cpu_model);

  TrimAndCollapseWhitespace(&text);

  Manufacturer manufacturer = GetManufacturer(text);

  // Remove everything between parentheses.
  Replace(&text, "\\([^)]*\\)", " ");

  // Remove special characters.
  Replace(&text, "\\$|®|™", " ");

  // Remove "GHz" patterns.
  Replace(&text, "(?i)@( )?\\d[.,]\\d+([~\\-]\\d[.,]\\d+)?( )?GHz\\b", "");
  Replace(&text, "(?i)\\b\\d[.,]\\d+([~\\-]\\d[.,]\\d+)?( )?GHz\\b", "");

  TrimAndCollapseWhitespace(&text);

  // Remove trailing special characters.
  Replace(&text, "(^| )?[@~\\-,.]$", "");

  // Remove filler words.
  Replace(&text, "(?i)\\bCPU\\b", "");
  Replace(&text, "(?i)\\bMobile\\b", "");
  Replace(&text, "(?i)\\bProcessor\\b", "");
  Replace(&text, "(?i)\\bSilicon\\b", "");
  Replace(&text, "(?i)\\bSOC\\b", "");
  Replace(&text, "(?i)\\bTechnology\\b", "");

  TrimAndCollapseWhitespace(&text);

  // Processing specific to each manufacturer.
  switch (manufacturer) {
    case Manufacturer::kAMD:
      ReplaceFirst(&text, "(?i).*?\\bAMD\\b", "");
      TrimAndCollapseWhitespace(&text);
      Replace(&text, "(?i)\\bFX -", "FX-");
      Replace(&text, "(?i)\\+( )?(AMD )?Radeon.*", "");
      Replace(&text,
              "(?i)\\b(RADEON )?R\\d+, \\d+ COMPUTE CORES \\d+C\\+\\d+G\\b",
              "");
      Replace(&text, "(?i)\\bwith (AMD )?Radeon.*", "");
      Replace(&text, "(?i)\\bw\\/( )?(AMD )?Radeon.*", "");
      Replace(&text, "(?i)\\bRadeon.*", "");

      Replace(&text, "(?i)\\b\\w+( |-)Core\\b", "");
      Replace(&text, "(?i)\\b\\d+-Core(s)?\\b", "");

      Replace(&text, "(?i)\\bAPU\\b", "");
      Replace(&text, "(?i)\\bCreator Edition\\b", "");
      Replace(&text, "(?i)\\bDesktop Kit\\b", "");

      Replace(&text, "(?i)\\b(3250C) 15W\\b", "\\1");
      break;
    case Manufacturer::kApple:
      ReplaceFirst(&text, "(?i).*?\\bApple\\b", "");
      break;
    case Manufacturer::kIntel:
      ReplaceFirst(&text, "(?i).*?\\bIntel\\b", "");
      TrimAndCollapseWhitespace(&text);
      Replace(&text, "(?i)\\b(Core)(2)\\b", "\\1 \\2");
      Replace(&text, "(?i)\\b(Core i\\d+)( )?-( )?", "\\1-");
      Replace(&text, "(?i)\\b(Core i\\d+) (M) (\\d+)\\b", "\\1-\\3\\2");
      Replace(&text, "(?i)\\b(Core i\\d+) ([LQU]) (\\d+)\\b", "\\1-\\3\\2M");
      Replace(&text, "(?i)\\b(Core i\\d+) (\\d+)\\b", "\\1-\\2");
      Replace(&text, "(?i)\\b(Celeron|Pentium) Dual(-Core)?\\b", "\\1");
      Replace(&text, "\\b0+$", "");
      break;
    default:
      // For all other manufacturers including kUnknown, we just return an
      // empty model string.
      return {manufacturer, ""};
  }

  TrimAndCollapseWhitespace(&text);
  return {manufacturer, text};
}

namespace {
std::atomic<Tier> g_tier_cached{Tier::kUnknown};
}  // anonymous namespace

void Initialize() {
  if (base::FeatureList::IsEnabled(blink::features::kCpuPerformance)) {
    // Use a simple, default implementation which is non-blocking, so that we
    // have a reasonable computed value in the unlikely case that the tier is
    // fetched by renderer initialization before the task executes.
    g_tier_cached.store(GetTierFromCores(base::SysInfo::NumberOfProcessors()));
    // Post the task that will update the tier asynchronously, using the more
    // accurate (but potentially blocking) implementation. Note that this task
    // is a "MayBlock" because of base::SysInfo::CPUModelName(), which on Linux
    // reads /proc/cpuinfo.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce([]() {
          Tier tier = GetTierFromCpuInfo(base::SysInfo::CPUModelName(),
                                         base::SysInfo::NumberOfProcessors());
          g_tier_cached.store(tier);
        }));
  }
}

Tier GetTier() {
  return g_tier_cached.load();
}

Tier GetTierFromCores(int cores) {
  if (cores >= 1 && cores <= 2) {
    return Tier::kLow;
  } else if (cores >= 3 && cores <= 4) {
    return Tier::kMid;
  } else if (cores >= 5 && cores <= 12) {
    return Tier::kHigh;
  } else if (cores >= 13) {
    return Tier::kUltra;
  }
  return Tier::kUnknown;
}

Tier GetTierFromCpuInfo(std::string_view cpu_model, int cores) {
  // Number of cores is unknown.
  if (cores <= 0) {
    return Tier::kUnknown;
  }

  if (cores <= 1) {
    return Tier::kLow;
  }

  auto [manufacturer, model] = SplitCpuModel(cpu_model);

  if (cores <= 4) {
    switch (manufacturer) {
      case Manufacturer::kAMD:
        // 2 cores, K8 + K10 + Mobile Trinity
        if (cores == 2 &&
            (Search(model, "^Athlon 64\\b") || Search(model, "^Athlon II\\b") ||
             Search(model, "^Athlon X2\\b") || Search(model, "^Phenom II\\b") ||
             Search(model, "^Sempron X2\\b") ||
             Search(model, "^Turion II\\b") || Search(model, "^Turion X2\\b") ||
             // Llano (~2011)
             Search(model, "^(A4|E2)-[3]\\d\\d\\d[A-Z]*\\b") ||
             // Trinity (~2012)
             Search(model, "^(A4|A6)-[4]\\d\\d\\dM[A-Z]*\\b"))) {
          return Tier::kLow;
        }
        // 2 cores, Bobcat + Jaguar
        if (cores == 2 && (Search(model, "^(C|E|E1|E2|T|Z)-\\w*\\b") ||
                           Search(model, "^(A4)-[1]\\d\\d\\d[A-Z]*\\b") ||
                           Search(model, "^(GX)-[2]\\d\\d[A-Z]*\\b") ||
                           Search(model, "^Sempron 2650\\b"))) {
          return Tier::kLow;
        }
        // 2 cores, Entry-Level Stoney Ridge 2019 6W Re-Release
        if (cores == 2 && Search(model, "^A4-9120[Ce]\\b")) {
          return Tier::kLow;
        }
        // 4 cores, >= Zen
        if (cores == 4 && Search(model, "^Ryzen\\b") &&
            // not 2 cores, Zen (~2017)
            !Search(model, "^Ryzen 3 Pro 2100GE\\b") &&
            !Search(model, "^Ryzen 3 Pro 3050GE\\b") &&
            !Search(model, "^Ryzen 3 2200U\\b") &&
            !Search(model, "^Ryzen 3 3200U\\b") &&
            !Search(model, "^Ryzen 3 3250[UC]\\b") &&
            !Search(model, "^Ryzen Embedded R[1]\\d\\d\\d[A-Z]*\\b") &&
            // not 2 cores, Zen+ (~2018)
            !Search(model, "^Ryzen Embedded R2312\\b")) {
          return Tier::kHigh;
        }
        break;
      case Manufacturer::kIntel:
        // 2-4 cores, E-Core, Bonnell + Saltwell
        if (cores >= 2 && cores <= 4 &&
            // Silverthorne (~2008)
            (Search(model, "^Atom (Z5)\\d\\d[A-Z]*\\b") ||
             // Diamondville (~2008)
             Search(model, "^Atom (2|3|N2)\\d\\d[A-Z]*\\b") ||
             // Pineview (~2010)
             Search(model, "^Atom (D4|N4|D5|N5)\\d\\d[A-Z]*\\b") ||
             // Tunnel Creek (~2010) / Stellarton (~2010)
             Search(model, "^Atom (E6)\\d\\d[A-Z]*\\b") ||
             // Lincroft (~2010)
             Search(model, "^Atom (Z6)\\d\\d[A-Z]*\\b") ||
             // Cedarview (~2011)
             Search(model, "^Atom (D2|N2)\\d\\d\\d[A-Z]*\\b") ||
             // Penwell (~2012) + Cloverview (~2012)
             Search(model, "^Atom (Z2)\\d\\d\\d[A-Z]*\\b"))) {
          return Tier::kLow;
        }
        // 2-4 cores, E-Core, Silvermont + Airmont
        if (cores >= 2 && cores <= 4 &&
            // Bay Trail-D (~2013)
            (Search(model, "^Celeron (J1)\\d\\d\\d[A-Z]*\\b") ||
             Search(model, "^Pentium (J2)\\d\\d\\d[A-Z]*\\b") ||
             // Avoton (~2013) / Rangeley (~2013)
             Search(model, "^Atom (C2)[35]\\d\\d[A-Z]*\\b") ||
             // Bay Trail-I (~2013)
             Search(model, "^Atom (E3)[8]\\d\\d[A-Z]*\\b") ||
             // Bay Trail-M (~2013)
             Search(model, "^Celeron (N2)\\d\\d\\d[A-Z]*\\b") ||
             Search(model, "^Pentium (N3)[5]\\d\\d[A-Z]*\\b") ||
             // Bay Trail-T (~2013) + Merrifield (~2014) + Moorefield (~2014)
             Search(model, "^Atom (Z3)\\d\\d\\d[A-Z]*\\b") ||
             // Braswell (~2016)
             Search(model, "^Atom x[5]-(E8)\\d\\d\\d[A-Z]*\\b") ||
             Search(model, "^Celeron (J3|N3)[01]\\d\\d[A-Z]*\\b") ||
             Search(model, "^Pentium (J3|N3)[7]\\d\\d[A-Z]*\\b") ||
             // Cherry Trail-T (~2015)
             Search(model, "^Atom x[57]-(Z8)\\d\\d\\d[A-Z]*\\b"))) {
          return Tier::kLow;
        }
        // 2 cores, E-Core, Goldmont
        if (cores == 2 &&
            // Apollo Lake (~2016)
            (Search(model, "^Atom x[5]-(A3|E3)\\d\\d\\d[A-Z]*\\b") ||
             Search(model, "^Celeron (J3|N3)[34]\\d\\d[A-Z]*\\b") ||
             // Denverton (~2017)
             Search(model, "^Atom (C3)\\d\\d\\d[A-Z]*\\b"))) {
          return Tier::kLow;
        }
        // 2 cores, P-Core, Merom + Penryn
        if (cores == 2 &&
            // Merom (~2006) + Conroe (~2006) + Penryn (~2007)
            (Search(model, "^Celeron (E1|SU2|T1|T3)\\d\\d\\d[A-Z]*\\b") ||
             Search(model, "^Core 2 Duo\\b") ||
             Search(model, "^Core 2 Extreme\\b") ||
             Search(model,
                    "^Pentium (E2|SU2|SU4|T2|T3|T4)\\d\\d\\d[A-Z]*\\b") ||
             Search(model, "^Xeon (3|E3|L3)\\d\\d\\d[A-Z]*\\b"))) {
          return Tier::kLow;
        }
        // 2 cores, P-Core, Nehalem + Westmere
        if (cores == 2 &&
            // Arrandale (~2010)
            (Search(model, "^Celeron (P4|U3)\\d\\d\\d[A-Z]*\\b") ||
             Search(model, "^Pentium (P6|U5)\\d\\d\\d[A-Z]*\\b"))) {
          return Tier::kLow;
        }
        // 2 cores, P-Core, Mobile Sandy Bridge + Mobile Ivy Bridge
        if (cores == 2 &&
            // Sandy Bridge-M (~2011)
            (Search(model, "^Celeron (7|8|B7|B8)\\d\\d[A-Z]*\\b") ||
             Search(model, "^Pentium (9|B9)\\d\\d[A-Z]*\\b") ||
             // Ivy Bridge-M (~2012)
             Search(model, "^Celeron (1)\\d\\d\\d[A-Z]*\\b") ||
             Search(model, "^Pentium (2|A1)\\d\\d\\d[A-Z]*\\b"))) {
          return Tier::kLow;
        }
        // 4 cores, E-Core, >= Gracemont
        if (cores == 4 && (Search(model, "^N\\d\\d+\\b") ||
                           // Alder Lake-N (~2023)
                           Search(model, "^Atom x7425E\\b"))) {
          return Tier::kHigh;
        }
        break;
      default:
        if (cores <= 2) {
          // Any other manufacturer
          return Tier::kLow;
        }
        break;
    }
    return Tier::kMid;
  }

  if (cores <= 10) {
    switch (manufacturer) {
      case Manufacturer::kApple:
        // 8+ cores, M-series
        if (cores >= 8 && Search(model, "^M\\d+\\b")) {
          return Tier::kUltra;
        }
        break;
      case Manufacturer::kIntel:
        // 8+ cores, >= Meteor Lake
        if (cores >= 8 && Search(model, "^Core Ultra\\b")) {
          return Tier::kUltra;
        }
        break;
      default:
        break;
    }
    return Tier::kHigh;
  }

  return Tier::kUltra;
}

}  // namespace content::cpu_performance
