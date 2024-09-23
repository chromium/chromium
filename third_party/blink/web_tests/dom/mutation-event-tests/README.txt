
See crbug.com/1446498 for more context.

The tests in this sub-folder only test Mutation Events. They were relocated here
from their original location (keeping the relative path structure) so that when
Mutation Events are removed, this entire folder can simply be deleted.

See TestExpectations under "Mutation Events tests from here" for a list of tests
that use mutation events and need special care.

