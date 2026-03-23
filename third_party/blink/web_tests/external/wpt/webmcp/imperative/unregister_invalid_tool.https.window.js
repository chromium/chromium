'use strict';

test(() => {
  assert_throws_dom(
    'InvalidStateError',
    () => {
      navigator.modelContext.unregisterTool('empty');
    },
    "unregisterTool which doesn't exist",
  );
}, "unregisterTool which doesn't exist");
