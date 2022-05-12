// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer for content/renderer

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <random>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/test/fuzzer/fuzzer_support.h"
#include "testing/libfuzzer/libfuzzer_exports.h"

extern "C" size_t LLVMFuzzerMutate(uint8_t* Data, size_t Size, size_t MaxSize);

namespace content {

using Random = std::mt19937;

// Pick random element out of stream.
template <typename T>
class ReservoirSampler {
 public:
  explicit ReservoirSampler(Random* rnd) : rnd_(rnd), n_(0) {}
  ReservoirSampler(Random* rnd, const T& t) : rnd_(rnd), t_(t), n_(0) {}

  inline bool empty() const { return !n_; }
  inline const T& get() const { return t_; }

  inline void operator()(const T& t) {
    n_++;
    // take with 1/n probability.
    if ((*rnd_)() % n_ == 0) {
      t_ = t;
    }
  }

 private:
  Random* const rnd_;
  T t_;
  int n_;
};

// Invoke libFuzzer's mutate on std::string.
static size_t MutateString(std::string* str, size_t maxLen) {
  static std::vector<uint8_t> v;
  v.resize(maxLen);
  size_t inLen = std::min(maxLen, str->length());
  memcpy(v.data(), str->data(), inLen);
  size_t len =
      LLVMFuzzerMutate(reinterpret_cast<uint8_t*>(v.data()), inLen, maxLen);
  if (!len)
    return 0;
  *str = std::string(reinterpret_cast<char*>(v.data()), len);
  return len;
}

class NodeList;
class Element;
class Text;

class Node {
 public:
  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  virtual ~Node() {}
  static std::unique_ptr<Node> CreateRandom(Random* rnd);

  virtual bool IsElement() const { return false; }
  virtual bool IsText() const { return false; }

  Element* AsElement() {
    CHECK(IsElement());
    return reinterpret_cast<Element*>(this);
  }
  Text* AsText() {
    CHECK(IsText());
    return reinterpret_cast<Text*>(this);
  }

 protected:
  Node() {}

  virtual std::unique_ptr<base::Value> ToJson() = 0;
  virtual void ParseJson(const base::DictionaryValue& dict) = 0;
  virtual void WriteHtml(std::string* out) = 0;

 private:
  friend class NodeList;
  static std::unique_ptr<Node> ParseJson(const base::Value& value);
};

using Attrs = std::map<std::string, std::string>;
using AttrPosition = std::pair<Attrs*, Attrs::iterator>;

class NodeList : public std::vector<std::unique_ptr<Node>> {
 public:
  // Position inside the tree. Iterator could be end().
  using NodePosition = std::pair<NodeList*, NodeList::iterator>;

  NodeList() {}

  NodeList(const NodeList&) = delete;
  NodeList& operator=(const NodeList&) = delete;

  static std::unique_ptr<NodeList> ParseJsonString(const uint8_t* data,
                                                   size_t size) {
    auto nodes = std::make_unique<NodeList>();

    absl::optional<base::Value> value(base::JSONReader::Read(
        std::string(reinterpret_cast<const char*>(data), size)));
    if (value)
      nodes->ParseJson(*value);

    return nodes;
  }

  std::unique_ptr<base::Value> ToJson() const {
    std::unique_ptr<base::ListValue> result(new base::ListValue());
    for (const auto& node : *this) {
      result->GetList().Append(base::Value::FromUniquePtrValue(node->ToJson()));
    }
    return std::move(result);
  }

  void ToJsonString(std::string* out) const {
    auto json = ToJson();
    CHECK(json);
    bool succ = base::JSONWriter::Write(*json, out);
    CHECK(succ);
  }

  void WriteHtml(std::string* out) const {
    for (const auto& node : *this) {
      node->WriteHtml(out);
    }
  }

  // Traverses the tree an invokes fn on all NodePositions.
  template <typename Fn>
  void WalkTree(Fn fn);

  template <typename FilterFn>
  NodePosition PickRandomPos(Random* rnd, FilterFn filter);

  AttrPosition PickRandomAttribute(Random* rnd);

  void ParseJson(const base::Value& value) {
    if (!value.is_list()) {
      return;
    }

    for (const auto& listItem : value.GetList()) {
      std::unique_ptr<Node> node(Node::ParseJson(listItem));
      if (node) {
        push_back(std::move(node));
      }
    }
  }

 private:
  friend class Element;
};

class Element : public Node {
 public:
  static std::unique_ptr<Node> CreateRandom(Random* rnd) {
    static std::vector<std::string> tagNames;
    if (tagNames.empty()) {
      tagNames.insert(
          tagNames.begin(),
          {"a",        "abbr",       "address",  "area",       "article",
           "aside",    "audio",      "b",        "base",       "bdi",
           "bdo",      "blockquote", "body",     "br",         "button",
           "canvas",   "caption",    "cite",     "code",       "col",
           "colgroup", "command",    "datalist", "dd",         "del",
           "details",  "dfn",        "div",      "dl",         "dt",
           "em",       "embed",      "fieldset", "figcaption", "figure",
           "footer",   "form",       "h1",       "h2",         "h3",
           "h4",       "h5",         "h6",       "head",       "header",
           "hgroup",   "hr",         "html",     "i",          "iframe",
           "img",      "input",      "ins",      "kbd",        "keygen",
           "label",    "legend",     "li",       "link",       "map",
           "mark",     "menu",       "meta",     "meter",      "nav",
           "noscript", "object",     "ol",       "optgroup",   "option",
           "output",   "p",          "param",    "pre",        "progress",
           "q",        "rp",         "rt",       "ruby",       "s",
           "samp",     "script",     "section",  "small",      "source",
           "span",     "strong",     "style",    "sub",        "summary",
           "sup",      "table",      "tbody",    "td",         "textarea",
           "tfoot",    "th",         "thead",    "time",       "title",
           "tr",       "track",      "u",        "ul",         "var",
           "video",    "wbr"});
    }

    return std::unique_ptr<Element>(
        new Element(tagNames[(*rnd)() % tagNames.size()]));
  }

  Element(const Element&) = delete;
  Element& operator=(const Element&) = delete;

  bool IsElement() const override { return true; }

  void SetAttr(const std::string& attr, const std::string& value) {
    attrs_[attr] = value;
  }

 protected:
  void WriteHtml(std::string* out) override {
    *out += "<" + tag_name_;
    if (!attrs_.empty()) {
      for (auto p : attrs_) {
        *out += " ";
        *out += p.first;
        *out += "='";
        *out += p.second;
        *out += "'";
      }
    }
    *out += ">";
    children_.WriteHtml(out);
    *out += "</" + tag_name_ + ">";
  }

  std::unique_ptr<base::Value> ToJson() override {
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

    dict->SetString("e", tag_name_);
    if (!children_.empty())
      dict->Set("c", children_.ToJson());
    if (!attrs_.empty()) {
      auto attrsDict =
          std::unique_ptr<base::DictionaryValue>(new base::DictionaryValue());
      for (const auto& pair : attrs_) {
        attrsDict->SetString(pair.first, pair.second);
      }
      dict->Set("a", std::move(attrsDict));
    }

    return std::move(dict);
  }

 protected:
  void ParseJson(const base::DictionaryValue& dict) override {
    CHECK(dict.FindKey("e"));
    dict.GetString("e", &tag_name_);

    const base::ListValue* list;
    if (dict.GetList("c", &list))
      children_.ParseJson(*list);

    const base::DictionaryValue* attrsDict;
    if (dict.GetDictionary("a", &attrsDict)) {
      for (base::DictionaryValue::Iterator it(*attrsDict); !it.IsAtEnd();
           it.Advance()) {
        if (it.value().is_string())
          attrs_[it.key()] = it.value().GetString();
      }
    }
  }

 private:
  friend class Node;
  friend class NodeList;

  Element() : tag_name_("__invalid__") {}
  explicit Element(const std::string& tag_name) : tag_name_(tag_name) {}

  std::string tag_name_;
  NodeList children_;
  Attrs attrs_;
};

class Text : public Node {
 public:
  static std::unique_ptr<Node> CreateRandom(Random* rnd) {
    return std::unique_ptr<Node>(new Text);
  }

  Text(const Text&) = delete;
  Text& operator=(const Text&) = delete;

  size_t MutateText() {
    // TODO(aizatsky): constant?
    return MutateString(&text_, 32);
  }

 protected:
  void WriteHtml(std::string* out) override { *out += text_; }

  std::unique_ptr<base::Value> ToJson() override {
    base::DictionaryValue* result = new base::DictionaryValue();
    result->SetString("t", text_);
    return std::unique_ptr<base::Value>(result);
  }

  void ParseJson(const base::DictionaryValue& dict) override {
    CHECK(dict.FindKey("t"));
    dict.GetString("t", &text_);
  }

  bool IsText() const override { return true; }

 private:
  friend class Node;
  Text() {}
  explicit Text(std::string text) : text_(text) {}

  std::string text_;
};

template <typename Fn>
void NodeList::WalkTree(Fn fn) {
  for (iterator i = begin(); i != end(); ++i) {
    fn(NodePosition(this, i));
    value_type& node = *i;
    if (node->IsElement()) {
      node->AsElement()->children_.WalkTree(fn);
    }
  }
  fn(NodePosition(this, end()));
}

template <typename FilterFn>
NodeList::NodePosition NodeList::PickRandomPos(Random* rnd,
                                               FilterFn filter_fn) {
  ReservoirSampler<NodePosition> sampler(rnd, NodePosition(nullptr, end()));

  WalkTree([&filter_fn, &sampler](const NodePosition& p) {
    if (filter_fn(p))
      sampler(p);
  });

  return sampler.get();
}

AttrPosition NodeList::PickRandomAttribute(Random* rnd) {
  ReservoirSampler<AttrPosition> sampler(
      rnd, AttrPosition(nullptr, Attrs::iterator()));

  WalkTree([&sampler](const NodePosition& p) {
    if (p.second == p.first->end() || !(*p.second)->IsElement())
      return;

    Element* e = (*p.second)->AsElement();
    for (Attrs::iterator i = e->attrs_.begin(); i != e->attrs_.end(); ++i) {
      sampler(make_pair(&e->attrs_, i));
    }
  });

  return sampler.get();
}

std::unique_ptr<Node> Node::CreateRandom(Random* rnd) {
  switch ((*rnd)() % 2) {
    default:
      LOG(FATAL) << "SHOULD NOT HAPPEN";
      return nullptr;
    case 0:
      return Element::CreateRandom(rnd);
    case 1:
      return Text::CreateRandom(rnd);
  }
}

std::unique_ptr<Node> Node::ParseJson(const base::Value& value) {
  const base::DictionaryValue* dict;
  if (!value.GetAsDictionary(&dict)) {
    return nullptr;
  }

  std::unique_ptr<Node> node;

  if (dict->FindKey("t")) {
    node.reset(new Text());
  } else if (dict->FindKey("e")) {
    node.reset(new Element());
  } else {
    LOG(ERROR) << "Bad node";
  }

  if (node)
    node->ParseJson(*dict);

  return node;
}

static bool Mutate_InsertNode(NodeList* nodes, Random* rnd) {
  auto [list, pos] = nodes->PickRandomPos(
      rnd, [](const NodeList::NodePosition&) { return true; });

  list->insert(pos, Node::CreateRandom(rnd));
  return true;
}

static bool Mutate_Text(NodeList* nodes, Random* rnd) {
  auto [list, pos] =
      nodes->PickRandomPos(rnd, [](const NodeList::NodePosition& p) {
        return p.second != p.first->end() && (*p.second)->IsText();
      });

  if (!list)
    return false;

  return (*pos)->AsText()->MutateText();
}

static bool Mutate_DeleteNode(NodeList* nodes, Random* rnd) {
  auto [list, pos] =
      nodes->PickRandomPos(rnd, [](const NodeList::NodePosition& p) {
        return p.second != p.first->end();
      });

  if (list == nullptr)
    return false;

  list->erase(pos);

  return true;
}

static bool Mutate_AddAttribute(NodeList* nodes, Random* rnd) {
  static std::vector<std::string> attrNames;
  if (attrNames.empty()) {
    attrNames.insert(attrNames.begin(), {"accept",      "accept-charset",
                                         "accesskey",   "action",
                                         "align",       "alt",
                                         "async",       "autocomplete",
                                         "autofocus",   "autoplay",
                                         "autosave",    "bgcolor",
                                         "border",      "buffered",
                                         "challenge",   "charset",
                                         "checked",     "cite",
                                         "class",       "code",
                                         "codebase",    "color",
                                         "cols",        "colspan",
                                         "content",     "cointenteditable",
                                         "contextmenu", "controls",
                                         "data",        "data-a",
                                         "data-b",      "data-c",
                                         "datetime",    "default",
                                         "defer",       "dir",
                                         "dirname",     "disabled",
                                         "download",    "draggable",
                                         "dropzone",    "enctype",
                                         "for",         "form",
                                         "formaction",  "headers",
                                         "height",      "hidden",
                                         "high",        "href",
                                         "hreflang",    "http-equiv",
                                         "icon",        "id",
                                         "ismap",       "itemprop",
                                         "keytype",     "kind",
                                         "label",       "lang",
                                         "language",    "list",
                                         "loop",        "low",
                                         "manifest",    "max",
                                         "maxlength",   "media",
                                         "method",      "min",
                                         "multiple",    "muted",
                                         "name",        "novalidate",
                                         "open",        "optimum",
                                         "pattern",     "ping",
                                         "placeholder", "poster",
                                         "preload",     "radiogroup",
                                         "readonly",    "rel",
                                         "required",    "reversed",
                                         "rows",        "rowspan",
                                         "sandbox",     "scope",
                                         "scoped",      "seamless",
                                         "selected",    "shape",
                                         "size",        "sizes",
                                         "span",        "spellcheck",
                                         "src",         "srcdoc",
                                         "srclang",     "srcset",
                                         "start",       "step",
                                         "style",       "summary",
                                         "tabindex",    "Target",
                                         "title",       "type",
                                         "usemap",      "value",
                                         "width",       "wrap"});
  }

  auto [list, pos] =
      nodes->PickRandomPos(rnd, [](const NodeList::NodePosition& p) {
        return p.second != p.first->end() && (*p.second)->IsElement();
      });

  if (list == nullptr)
    return false;

  std::string name = attrNames[(*rnd)() % attrNames.size()];
  (*pos)->AsElement()->SetAttr(name, "");
  return true;
}

static bool Mutate_DeleteAttribute(NodeList* nodes, Random* rnd) {
  auto [attrs, pos] = nodes->PickRandomAttribute(rnd);

  if (attrs == nullptr)
    return false;

  attrs->erase(pos);
  return true;
}

static bool Mutate_AttributeValue(NodeList* nodes, Random* rnd) {
  auto [attrs, pos] = nodes->PickRandomAttribute(rnd);

  if (attrs == nullptr)
    return false;

  // TODO(aizatsky): constant
  return MutateString(&pos->second, 32);
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data,
                                          size_t size,
                                          size_t max_size,
                                          unsigned int Seed) {
  using NodeMutator = bool (*)(NodeList * list, Random * rnd);
  static std::vector<NodeMutator> mutators;
  if (mutators.empty()) {
    mutators.push_back(&Mutate_InsertNode);
    mutators.push_back(&Mutate_DeleteNode);
    mutators.push_back(&Mutate_Text);
    mutators.push_back(&Mutate_AddAttribute);
    mutators.push_back(&Mutate_DeleteAttribute);
    mutators.push_back(&Mutate_AttributeValue);
  }

  auto nodes = NodeList::ParseJsonString(data, size);

  Random rnd(Seed);
  NodeMutator mutator = mutators[rnd() % mutators.size()];

  if (!mutator(nodes.get(), &rnd)) {
    return 0;
  }

  std::string result;
  nodes->ToJsonString(&result);
  VLOG(1) << "mutatedjson:'" << result << "'";

  if (result.size() > max_size) {
    VLOG(1) << "**** OVERFLOW : " << result.size();
    return 0;
  }

  memcpy(data, result.data(), result.size());
  return result.size();
}

static Env* env = nullptr;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Environment has to be initialized in the same thread.
  if (env == nullptr)
    env = new Env();

  auto nodes = NodeList::ParseJsonString(data, size);
  std::string html;
  nodes->WriteHtml(&html);

  env->adapter->LoadHTML(html, "http://www.example.org");
  return 0;
}

}  // namespace content
