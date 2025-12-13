	# Imported Documentation Cache

## What is this directory?

This directory contains a local cache of documentation that is sourced from
external repositories (e.g., the standalone GN repository).

NOTE: This is a temporary solution so that coding agents can access vital docs
about GN (until there is consistent access to web fetch for all).

## Why does it exist?

The primary purpose of this cache is to provide a stable, in-tree knowledge
base for development tools and AI assistants. By having a local copy of this
essential documentation, tools can operate consistently without relying on
network access, and AI assistants can be instructed to use this high-quality,
version-controlled information to provide accurate and context-aware help for
Chromium development.

## How is it updated?

The contents of this directory are managed by the script `refresh_docs.py`
located in this same directory. This script fetches the documents listed in
its manifest from their canonical sources.

**Do not edit the files in this directory directly.** Your changes will be
overwritten the next time the refresh script is run. If you need to update
or add a document, please modify the `DOCUMENT_MANIFEST` within
`refresh_docs.py`.

## Note on AI Integration

The AI assistant's instructions for using this documentation are located in
`agents/prompts/chromium_knowledge_base.md`. It is recommended to review that
file for any necessary updates whenever the document manifest in
`refresh_docs.py` is changed.