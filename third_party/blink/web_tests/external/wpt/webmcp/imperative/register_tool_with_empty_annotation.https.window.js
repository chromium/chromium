'use strict';

test(() => {
  navigator.modelContext.registerTool({
    name: 'echo',
    description: 'echo input',
    execute: (obj) => obj.text,
    annotations: {
      // No `readOnlyHint` member.
    },
  });

  navigator.modelContext.unregisterTool('echo');
}, 'register tool with empty annotations');
