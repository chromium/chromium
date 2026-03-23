'use strict';

test(() => {
  navigator.modelContext.registerTool({
    name: 'empty',
    description: 'echo empty',
    execute: () => {},
  });

  navigator.modelContext.unregisterTool('empty');
}, 'register tool with only required params');
