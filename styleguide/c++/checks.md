# CHECK(), DCHECK() and NOTREACHED()

`CHECK()`, `DCHECK()` and `NOTREACHED()` are all used to ensure that invariants
hold.  They document (and verify) programmer expectations that either some
statement *always* holds true at the point of `(D)CHECK`ing or that a piece of
code is unreachable. They should not be used to validate data that is provided
by end-users or website developers. Such data is untrusted, and must be
validated by standard control flow.

An invariant that does not hold should be seen as Undefined Behavior, and
continuing past it puts the program into an unexpected state. This applies in
particular to `DCHECK()`s as they do not test anything in production and thus do
not stop the program from continuing with the invariant being violated. All
invariant failures should be seen as P1 bugs, regardless of their crash rate.
Continuing past an invariant failure can cause crashes and incorrect behaviour
for our users, but also frequently presents security vulnerabilities as
attackers may leverage the unexpected state to take control of the program. In
the future we may let the compiler assume and optimize around `DCHECK()`s
holding true in non-DCHECK builds using `__builtin_assume()`, which further
formalizes undefined behavior.

## Failures beyond Chromium's control

Failure can come from beyond Chromium's ability to control. These failures
should not be caught with invariants, but handled as part of regular control
flow. In the rare case where it's impossible to safely recover from failure use
`base::ImmediateCrash()` to terminate the process instead of using `CHECK()`
etc. Doing so avoids implying that the generated crash reports should be triaged
as bugs in Chromium. Fatally aborting is a last-resort measure.

We must be resilient to a bad prior release of Chromium which may have persisted
bad data to disk or a bad server-side rollout which may be sending us incorrect
or unexpected configs.

Note that wherever `CHECK()` is inappropriate, `DCHECK()` is inappropriate as
well. `DCHECK()` should still only be used for invariants. Ideally we'd have
better test coverage for failures created from outside Chromium's control.

Non-exhaustive list of failures beyond Chromium's control:

* *Exhausted resources:* Running out of memory, FD handles, etc. should be made
  unlikely to happen, but is not entirely within our control. When we can't
  gracefully degrade, use a non-asserting `base::ImmediateCrash()`.
* *Untrusted data:* Data provided by end users or website developers. Don't
  `CHECK()` for bad syntax, etc.
* *Serialized data out of sync with binary:* Any data persisted to disk may come
  from a past or future version of Chrome. Server data such as experiments
  should also not be verified with `CHECK()`s as a bad server-side rollout
  shouldn't be able to bring down the client. Note that you may `CHECK()` that
  data is valid after the caller should've validated it.
* *Disk corruption:* We should be able to recover from a bad disk read/write. Do
  not assume that the data comes from a current (or even past) version of
  Chromium. This includes preferences which are persisted to disk.
* *Data across security boundaries:* A compromised renderer should not be able
  to bring down the browser process (higher privilege). Bad IPC messages should
  be safely [rejected](../../docs/security/mojo.md#explicitly-reject-bad-input)
  by Chromium without the use of `base::ImmediateCrash()` or `CHECK()` etc.
  as part of normal control flow.
* *Bad/untrusted/changing driver, kernel API, hardware failure:* A misbehaving
  GPU driver may cause us to be unable to proceed. This is not an invariant
  failure. On platforms where we are wary that a kernel API may change without
  sufficient prior notice we should not `CHECK()` its result as we expect the
  rug to be pulled from under our feet. In the case of hardware failure we
  should not for instance `CHECK()` that a write succeeded.

In some cases (malware, ..., dll injection, etc.) third-party code outside of
our control can cause us to trigger various CHECK() failures, due to injecting
code or unexpected state into Chrome. These can create "weird machines" that are
useful to attackers. We don't remove CHECKs just to support them, though we may
handle these unexpected states if possible and necessary. Chromium is not
designed to run against arbitrary code modification.

## Invariant-verification mechanisms

Prefer `CHECK()` and `NOTREACHED()` over `DCHECK()`s as they ensure that if an
invariant fails, the program does not continue in an unexpected state, and we
hear about the failure either through a test failure or a crash report. This
helps prevent user harm such as security bugs when our software does what we did
not expect. Historically, `CHECK()` was seen as expensive but great effort and
care has gone into making the crash instructions nearly free on modern CPUs. Log
messages are discarded from `CHECK()`s in production builds but provide
additional information in debug and `DCHECK` builds.

`DCHECK()` (and `DCHECK_EQ()`, `DCHECK_LT()`, etc) provide a fallback mechanism
to check for invariants where the test being performed is too expensive (either
in terms of generated code size or performance) to verify in production builds.
The risk of depending on `DCHECK()` is that, since it disappears in production
builds, it's only verified in tests, on developer machines and a very small
subset of Canary builds. Any side effects intended to happen inside the
`DCHECK()` disappear from production along with it, and unexpected behaviour can
happen afterward as a result.

`NOTREACHED()` signals that a piece of code is intended to be unreachable while
also terminating if it is in fact reached, as if a `CHECK()` failure.

## Examples

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
// using NOTREACHED() while also making sure we hear about it.
switch (my_enum) {
  case A: return 1;
  case B: return 5;
  case C: return 3;
}
NOTREACHED();

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
}
```

## More cautious CHECK() / NOTREACHED() rollouts and DCHECK() upgrades

If you're not confident that an unexpected situation can't happen in practice,
an additional `base::NotFatalUntil::M120` argument after the condition may be
used to gather non-fatal diagnostics before turning fatal in a future milestone.
`CHECK()` and `NOTREACHED()` with a `base::NotFatalUntil` argument will provide
non-fatal crash reports before the fatal milestone is hit. They preserve and
upload logged arguments which is useful for debugging failures during rollout as
well.

Since these variants are non-fatal and do not terminate make best-effort
attempts to handle the situation, like an early return and try to reason about
that being at least "probably safe" in calling contexts. Do not use
`base::NotFatalUntil` if there's no reasonable way to recover from the invariant
failure (i.e. if this is wrong we're about to crash or hit a memory bug).

Any invariant failures should be resolved before turning fatal even if they only
fail for a very low number of users (above the noise floor). Once fatal they
will be invariants that we collectively trust to hold true (other code may be
rewritten with these assumptions).

Using non-fatal invariant validation is especially appropriate when there's low
pre-stable coverage. Specifically consider using these:

* When working on iOS code (low pre-stable coverage).
* Upgrading `DCHECK()s`.
* Working on code that's not flag guarded.

As `base::NotFatalUntil` automatically turns fatal, keep an extra eye on
automatically-filed bugs for failures in the wild. Discovered failures, like
other invariant failures, are high-priority issues. Once resolved, either by
handling the unexpected situation or making sure it no longer happens, the
milestone number should be bumped to allow for validation in stable channels
before turning fatal.

Failing instances should not downgrade to DCHECKs as that hides the ongoing
problem rather than resolving it. In rare exceptions you could use
`DUMP_WILL_BE_CHECK()` macros for similar semantics (report on failure) without
timeline expectations, though in this case you must also handle failure as best
you can as failures are known to happen.

## Alternatives in tests

For failures in tests, GoogleTest macros such as `EXPECT_*`, `ASSERT_*` or
`ADD_FAILURE()` are more appropriate than `CHECKing`. For production code:

* `LOG(DFATAL)` is fatal on bots running tests but only logs an error in
  production.
* `DLOG(FATAL)` is fatal on bots running tests and does nothing in production.

As these only cause tests to fail, they should be rarely used, and mostly exist
for pre-existing code. Prefer to write a test that covers these scenarios and
verify the code handles it, or use a fatal `CHECK()` to actually prevent the
case from happening.
