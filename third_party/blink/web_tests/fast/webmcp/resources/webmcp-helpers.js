async function waitForTool(name) {
  let tools = await document.modelContext.getTools();
  if (tools.some(t => t.name === name)) {
    return;
  }
  await new Promise(resolve => {
    const handler = async () => {
      let tools = await document.modelContext.getTools();
      if (tools.some(t => t.name === name)) {
        document.modelContext.removeEventListener('toolchange', handler);
        resolve();
      }
    };
    document.modelContext.addEventListener('toolchange', handler);
  });
}
