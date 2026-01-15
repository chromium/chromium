# Technical Design Specs

This folder contains technical design specifications for the Agent Browser Protocol (ABP) Chromium fork.

## Project Overview

A Chromium fork designed for engine-native browser control, exposing a REST-based Agent Browser Protocol that allows agents to interact with the browser at the C++ engine level, bypassing extensions and CDP entirely.

## Documents

| Document | Description | Status |
|----------|-------------|--------|
| [agent-browser-protocol.md](./agent-browser-protocol.md) | Core ABP architecture and design | Draft |
| [API.md](./API.md) | Complete REST API specification | Draft |
| [mcp.md](./mcp.md) | MCP server for AI agent integration | Draft |
| http-server-implementation.md | HTTP server implementation details | TODO |
| input-injection-design.md | Input injection at engine level | TODO |
| dom-access-architecture.md | Direct DOM access patterns | TODO |

## Key Principles

1. **Engine-Native**: All operations happen at the C++ level, not through JavaScript bridges
2. **REST-First**: Simple HTTP API that any agent framework can consume
3. **Minimal Overhead**: Direct access means lower latency than CDP
4. **Agent-Optimized**: API designed for AI agent use cases, not human debugging

## Contributing

When adding new design specs:

1. Create a new `.md` file in this directory
2. Update the table above
3. Link related documents
4. Include implementation phases with checkboxes
