# Annotation

[Rendered](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/annotation/README.md)

The `core/annotation` directory contains the implementation of Blink web
content annotations.

This component allows Blink features and embedders to attach annotations to
content in a page rendered by Blink. It provides support only for attachment
and interaction with markers in the web content. The actual data and rendering
of any annotation _content_ (e.g. the text of a user note) is not in scope for
this component and must be implemented by the embedder.

On a feature level, `core/annotation` provides:

  * Selectors - a way for a client to specify which part of the DOM an
    annotation should be attached to. E.g. "The text string 'brown dog'
    prefixed by 'The quick' and suffixed by 'jumped over'".
  * In-page markers of the content that an annotation is attached to (e.g.
    highlight annotated text).
  * User interaction with annotation markers - e.g. providing notification to
    client when a marker is clicked or invokes the context menu.
  * Client interaction with annotation markers - e.g. scroll marker into view,
    get notified when marked content is removed from DOM or changes.

## Use Cases

Current consumers of `core/annotation` are:

  * [Text Fragment Directive](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h;l=27;drc=b8dc40b0a445983b75db39504cfdd66c304d7dca)
    - Also known as scroll-to-text, allows links to scroll to a specified text snipped on loading.
  * [Shared Highlighting](https://source.chromium.org/chromium/chromium/src/+/main:components/shared_highlighting/;drc=92e2d6aeefc1242e1b11467b994e4bfd14d17d20)
    which enables generating and sharing scroll-to-text links.
  * [User Notes](https://source.chromium.org/chromium/chromium/src/+/main:components/user_notes/;drc=d4f15fdf843b097546d7d8b1cfa715f094a01a33)
    which enables users to make notes on web pages.

## Concepts

* _Annotation_: in a very general sense, content added to a page that is not part
of the Document as authored by its creator. Examples might include: users
attaching notes to images or text snippets, making highlights or attaching
reactions/emojis, users or services attaching metadata like "time to read" or
ratings, etc.

* _Annotation Agent_: An object in Blink used to support annotating content. An
agent does not implement any kind of annotation content or semantics; it
provides a means by which a client implementing some form of annotation can
attach itself to specific content in a page, mark/highlight it, and provides
the client with functionality and notifications to interact with the attachment.

* _Agent Container_: Every agent is created by and stored in a container. When
removed, the agent is disposed and all its effects (e.g. highlights) are
removed. Containers are owned by a Document and created lazily on demand.
Clients interact with annotation agents via the container; that is, by binding
to the AnnotationAgentContainer mojo interface and requesting new
AnnotationAgents from it.

* _Selector_: A selector is used to attach an annotation to some content in a
page. A selector has enough information to uniquely identify an intended
instance of content on a page, for example, a text selector might include the
snippet of text a note is attached to with some prefix/suffix context to
disambiguate that snippet from other instances of the same text.

* _Attachment_: Is the process by which an agent invokes a selector to find a
range of DOM that matches the selector's parameters. If such a range is found,
the agent is considered _attached_. Attachment may fail, for example, if the
text of the page has changed since the annotation was created and no longer
exists. In this case, the agent is _unattached_.

## Design

TODO(bokan)
