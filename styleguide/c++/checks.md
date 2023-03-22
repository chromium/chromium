# CHECK(), DCHECK(), NOTREACHED_NORETURN() and NOTREACHED()

`CHECK()`, `DCHECK()`, `NOTREACHED_NORETURN()` and `NOTREACHED()` are all used
to ensure that invariants hold.  They document (and verify) programmer
expectations that either some statement *always* holds true at the point of
`(D)CHECK`ing or that a piece of code is unreachable. They should not be used to
validate data that is provided by end-users or website developers. Such data is
untrusted, and must be validated by standard control flow.

An invariant that does not hold should be seen as Undefined Behavior, and
continuing past it puts the program into an unexpected state. This applies in
particular to `DCHECK()` and `NOTREACHED()` since they do not test anything in
production and thus do not stop the program from continuing with the invariant
being violated. All invariant failures should be seen as P1 bugs, regardless of
their crash rate. Continuing past an invariant failure can cause crashes and
incorrect behaviour for our users, but also frequently presents security
vulnerabilities as attackers may leverage the unexpected state to take control
of the program. In the future we may let the compiler assume and optimize around
`DCHECK()`s holding true in non-DCHECK builds using `__builtin_assume()`, which
further formalizes undefined behavior.

Prefer `CHECK()` and `NOTREACHED_NORETURN()` as they ensure that if an invariant
fails, the program does not continue in an unexpected state, and we hear about
the failure either through a test failure or a crash report. This helps prevent
user harm such as security bugs when our software does what we did not expect.
Historically, `CHECK()` was seen as expensive but great effort and care has gone
into making the crash instructions nearly free on modern CPUs. Log messages are
discarded from `CHECK()`s in production builds but provide additional
information in debug and `DCHECK` builds.

`DCHECK()` (and `DCHECK_EQ()`, `DCHECK_LT()`, etc) provide a fallback mechanism
to check for invariants where the test being performed is too expensive (either
in terms of generated code size or performance) to verify in production builds.
The risk of depending on `DCHECK()` is that, since it disappears in production
builds, it only exists in tests, on developer machines and a very small subset
of Canary builds. Any side effects intended to happen inside the `DCHECK()`
disappear from production along with it, and unexpected behaviour can happen
afterward as a result.

`NOTREACHED_NORETURN()` signals that a piece of code is intended to be
unreachable, and lets the compiler optimize based on that fact, while
terminating if it is in fact reached. Like `CHECK()`, this ensures we hear about
the invariant failing through a test failure or a crash report, and prevents
user harm. Historically we used the shorter `NOTREACHED()` to indicate code was
unreachable, however it disappears in production builds and we have observed
that these are in fact commonly reached. Prefer `NOTREACHED_NORETURN()` in new
code, while we migrate the preexisting cases to it with care. See
https://crbug.com/851128.

Below are some examples to explore the choice of `CHECK()` and its variants:

```c++
// Good:
//
// Testing pointer equality is very cheap so write this as a CHECK. A security
// bug would happen afterward if the CHECK fails (in this case, on the next
// line).
auto it = container.find(key);
CHECK(it != container.end());
*it = Foo();

// Good:
//
// This is an expensive operation. Consider writing a test to provide coverage
// for this as well. DCHECK() is available as a fallback to verify the condition
// in tests and on a small subset of Canary builds.
DCHECK(|invoke an O(n^2) operation|);

// Good:
//
// This switch handles all cases, but enums can technically hold any integer
// value (even if all enum members are enumerated), so the compiler must try to
// handle other cases too. We can avoid dealing with values outside enums by
// using NOTREACHED_NORETURN() while also making sure we hear about it.
switch (my_enum) {
  case A: return 1;
  case B: return 5;
  case C: return 3;
}
NOTREACHED_NORETURN();

// Bad:
//
// Do not handle `DCHECK()` failures. Use `CHECK()` instead and omit the
// conditional below.
DCHECK(foo);  // Use CHECK() instead and omit conditional below.
if (!foo) {
  ...
}

// Bad:
//
// Use CHECK(bar); instead.
if (!bar) {
  NOTREACHED();
  return;
}
```

## Failures beyond Chromium's control

In some cases, a failure comes from beyond Chromium's ability to control, such
as unexpected out-of-memory conditions, a misbehaving driver, kernel API, or
hardware failure. Where it's impossible to safely recover from these failures,
use `base::ImmediateCrash()` to terminate the process instead of `CHECK()` etc.
Doing so avoids implying that the generated crash reports should be triaged as
bugs in Chromium.

Note that bad IPC messages should be safely rejected by Chromium without the use
of `base::ImmediateCrash()` or `CHECK()` etc. as part of normal control flow.

## Less fatal options

If an unexpected situation is happening, `base::debug::DumpWithoutCrashing()`
can be used to help debug in production. While this is usually not desirable in
the long term, it can be necessary for investigations.
`base::debug::DumpWithoutCrashing()` generates a crash report, but the code
continues on after, so be sure to handle the situation in a way that doesn't
leave the process in a bad state.

`SCOPED_CRASH_KEY_BOOL()`, `SCOPED_CRASH_KEY_NUMBER()`, etc. are macros in
`base/debug/crash_logging.h` that can be used ahead of
`base::debug::DumpWithoutCrashing()` to add additional data to the crash report.

For failures in tests, GoogleTest macros such as `EXPECT_*`, `ASSERT_*` or
`ADD_FAILURE()` are more appropriate than `CHECKing`. For production code:

* `LOG(DFATAL)` is fatal on bots running tests but only logs an error in
  production.
* `DLOG(FATAL)` is fatal on bots running tests and does nothing in production.

As these only cause tests to fail, they should be rarely used, and mostly exist
for pre-existing code. Prefer to write a test that covers these scenarios and
verify the code handles it, or use a fatal `CHECK()` to actually prevent the
case from happening.
