// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "skia/ext/switches.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/gfx/vector_icon_utils.h"

namespace {

using fuzztest::Arbitrary;
using fuzztest::Domain;
using fuzztest::ElementOf;
using fuzztest::Finite;
using fuzztest::FlatMap;
using fuzztest::InRange;
using fuzztest::Just;
using fuzztest::Map;
using fuzztest::NonEmpty;
using fuzztest::StructOf;
using fuzztest::VariantOf;
using fuzztest::VectorOf;

// Fuzztest does not yet support enums out of the box, but thankfully the
// `gfx::CommandType` enum is defined through a pair of macros that work very
// well for us. `DECLARE_VECTOR_COMMAND(x)` is supposed to be overridden like
// this before `DECLARE_VECTOR_COMMANDS` can be used. Dependency injection!
#define DECLARE_VECTOR_COMMAND(x) gfx::CommandType::x,

// That allows us to define a domain that contains only valid vector commands.
auto AnyCommandType() {
  return ElementOf({DECLARE_VECTOR_COMMANDS});
}

// Each command type has a specific number of args it expects, otherwise the
// command validation code CHECK-fails. We thus make sure to generate only
// valid sequences of `gfx::PathElement`s by packaging commands with the right
// number of arguments.
struct Command {
  gfx::CommandType type;
  std::vector<SkScalar> args;
};

// Returns the domain of all possible arguments for the given command.
//
// We need this to account for the fact that not all arguments are valid for
// all commands, and passing invalid arguments can trigger shallow CHECK
// failures that prevent deeper fuzzing.
Domain<std::vector<SkScalar>> AnyArgsForCommandType(gfx::CommandType type) {
  int args_count = gfx::GetCommandArgumentCount(type);
  switch (type) {
    case gfx::PATH_COLOR_ARGB:
      return VectorOf(InRange(SkScalar(0.0), SkScalar(255.0)))
          .WithSize(args_count);
    case gfx::CANVAS_DIMENSIONS:
      return VectorOf(InRange(SkScalar(1.0), SkScalar(1024.0)))
          .WithSize(args_count);
    default:
      return VectorOf(Finite<SkScalar>()).WithSize(args_count);
  }
}

Domain<Command> AnyCommandWithType(gfx::CommandType type) {
  return StructOf<Command>(Just(type), AnyArgsForCommandType(type));
}

// Returns the domain of all possible commands.
auto AnyCommand() {
  return FlatMap(AnyCommandWithType, AnyCommandType());
}

// Flattens the given `commands` into a sequence of path elements that can be
// passed to `PaintVectorIcon()`.
std::vector<gfx::PathElement> ConvertCommands(
    const std::vector<Command>& commands) {
  std::vector<gfx::PathElement> path;
  for (const auto& command : commands) {
    path.emplace_back(command.type);
    for (SkScalar arg : command.args) {
      path.emplace_back(arg);
    }
  }
  return path;
}

class PaintVectorIconFuzzTest {
 public:
  PaintVectorIconFuzzTest() {
    // `Init()` ignores its arguments on Windows, so init with nothing and add
    // switches later.
    CHECK(base::CommandLine::Init(0, nullptr));

    // Set command-line arguments correctly to avoid check failures down the
    // line.
    base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
    command_line.AppendSwitchASCII(switches::kTextContrast, "1.0");
    command_line.AppendSwitchASCII(switches::kTextGamma, "1.0");
  }

  void PaintVectorIcon(std::vector<Command> commands) {
    std::vector<gfx::PathElement> path = ConvertCommands(commands);

    // An icon can contain multiple representations. We do not fuzz the code
    // that chooses which representation to draw based on canvas size and scale,
    // and instead use a single representation.
    gfx::VectorIconRep rep(path.data(), path.size());
    gfx::VectorIcon icon(&rep, /*reps_size=*/1u, "icon");

    constexpr float kImageScale = 1.f;
    constexpr bool kIsOpaque = true;
    gfx::Canvas canvas(gfx::Size(1024, 1024), kImageScale, kIsOpaque);

    // The length of a single edge of the square icon, in device-independent
    // pixels.
    constexpr int kDipSize = 1024;
    constexpr SkColor kBlack = SkColorSetARGB(255, 0, 0, 0);
    gfx::PaintVectorIcon(&canvas, icon, kDipSize, kBlack);
  }
};

FUZZ_TEST_F(PaintVectorIconFuzzTest, PaintVectorIcon)
    .WithDomains(NonEmpty(VectorOf(AnyCommand())));

}  // namespace
