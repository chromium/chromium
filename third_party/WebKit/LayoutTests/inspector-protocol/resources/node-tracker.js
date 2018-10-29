(class NodeTracker {
  constructor(dp) {
    this._nodes = new Map();
    this._dp = dp;
    dp.DOM.onSetChildNodes(message => message.params.nodes.forEach(node => this._addNode(node)));
  }

  addDocumentNode(documentNode) {
    this._addNode(documentNode);
  }

  _addNode(node) {
    this._nodes.set(node.nodeId, node);
    if (node.children)
      node.children.forEach(node => this._addNode(node));
  }

  nodeForId(nodeId) {
    return this._nodes.get(nodeId) || null;
  }

  async nodeForBackendId(backendNodeId) {
    const response = await this._dp.DOM.pushNodesByBackendIdsToFrontend({backendNodeIds: [backendNodeId]});
    if (!response.result)
      throw new Error(JSON.stringify(response));
    return this.nodeForId(response.result.nodeIds[0]);
  }

  nodes() {
    return Array.from(this._nodes.values());
  }

  nodeIds() {
    return Array.from(this._nodes.keys());
  }
})
