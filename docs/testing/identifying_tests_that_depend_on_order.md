
# Fixing web test flakiness

We'd like to stamp out all the tests that have ordering dependencies. This helps
make the tests more reliable and, eventually, will make it so we can run tests
in a random order and avoid new ordering dependencies being introduced. To get
there, we need to weed out and fix all the existing ordering dependencies.

## Diagnosing test ordering flakiness

These are steps for diagnosing ordering flakiness once you have a test that you
believe depends on an earlier test running.

### Bisect test ordering

1. Run the tests such that the test in question fails.
2. Run `./tools/print_web_test_ordering.py` and save the output to a file. This
   outputs the tests run in the order they were run on each content_shell
   instance.
3. Create a file that contains only the tests run on that worker in the same
   order as in your saved output file. The last line in the file should be the
   failing test.
4. Run
   `./tools/bisect_web_test_ordering.py --test-list=path/to/file/from/step/3`

The bisect_web_test_ordering.py script should spit out a list of tests at the
end that causes the test to fail.

*** promo
At the moment bisect_web_test_ordering.py only allows you to find tests that
fail due to a previous test running. It's a small change to the script to make
it work for tests that pass due to a previous test running (i.e. to figure out
which test it depends on running before it). Contact ojan@chromium if you're
interested in adding that feature to the script.
***

### Manual bisect

Instead of running `bisect_web_test_ordering.py`, you can manually do the work
of step 4 above.

1. `run_web_tests.py --child-processes=1 --order=none --test-list=path/to/file/from/step/3`
2. If the test doesn't fail here, then the test itself is probably just flaky.
   If it does, remove some lines from the file and repeat step 1. Continue
   repeating until you've found the dependency. If the test fails when run by
   itself, but passes on the bots, that means that it depends on another test to
   pass. In this case, you need to generate the list of tests run by
   `run_web_tests.py --order=natural` and repeat this process to find which test
   causes the test in question to *pass* (e.g.
   [crbug.com/262793](https://crbug.com/262793)).
3. File a bug and give it the
   [LayoutTestOrdering](https://crbug.com/?q=label:LayoutTestOrdering) label,
   e.g. [crbug.com/262787](https://crbug.com/262787) or
   [crbug.com/262791](https://crbug.com/262791).

### Finding test ordering flakiness

#### Run tests in a random order and diagnose failures

1. Run `run_web_tests.py --order=random --no-retry`
2. Run `./tools/print_web_test_ordering.py` and save the output to a file. This
   outputs the tests run in the order they were run on each content_shell
   instance.
3. Run the diagnosing steps from above to figure out which tests

Run `run_web_tests.py --run-singly --no-retry`. This starts up a new
content_shell instance for each test. Tests that fail when run in isolation but
pass when run as part of the full test suite represent some state that we're not
properly resetting between test runs or some state that we're not properly
setting when starting up content_shell. You might want to run with
`--timeout-ms=60000` to weed out tests that timeout due to waiting on
content_shell startup time.

#### Diagnose especially flaky tests

1. Load
   https://test-results.appspot.com/dashboards/overview.html#group=%40ToT%20Blink&flipCount=12
2. Tweak the flakiness threshold to the desired level of flakiness.
3. Click on *webkit_tests* to get that list of flaky tests.
4. Diagnose the source of flakiness for that test.
