//
// Created by zpj on 2/7/23.
//

#include "third_party/blink/renderer/core/page/dom_path_utils.h"

#include <algorithm>
#include <set>
#include <sstream>
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/css/dom_window_css.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_collection.h"

using namespace std;

namespace blink {

std::string DomPathUtils::GetCssSelector(Node* node, bool optimized) {
    std::string path;
    if (!node || !node->IsElementNode()) {
        return path;
    }
    std::vector<Step*> steps;
    Node* contextNode = node;
    while (contextNode != nullptr) {
        auto* element = DynamicTo<Element>(contextNode);
        if (!element) {
            break;
        }
        Step* step = cssPathStep(element, optimized, contextNode == node);
        if (!step) {
            break;
        }
        steps.push_back(step);
        if (step->optimized) {
            break;
        }
        contextNode = contextNode->parentNode();
    }
    for (int i = steps.size() - 1; i >= 0; i--) {
        path += steps[i]->value;
        if (i > 0) {
            path += " > ";
        }
    }
    return path;
}

std::vector<std::string> DomPathUtils::prefixedElementClassNames(Element* node) {
    std::vector<std::string> names;
    std::string classAttribute = node->getAttribute(html_names::kClassAttr).Utf8();
    if (classAttribute.empty()) {
        return names;
    }

    istringstream ss(classAttribute);
    string name;
    while(ss >> name) {
    	names.push_back("$" + name);
    }
    return names;
}

std::string DomPathUtils::idSelector(std::string& id) {
    String idStr(id);
    return "#" + DOMWindowCSS::escape(idStr).Utf8();
}

DomPathUtils::Step* DomPathUtils::cssPathStep(Element* node, bool optimized, bool isTargetNode) {
    std::string id = node->getAttribute(html_names::kIdAttr).Utf8();
    if (optimized) {
        if (!id.empty()) {
            return new Step(idSelector(id), true);
        }
    }

    std::string nodeNameLower = node->nodeName().Utf8();
    transform(nodeNameLower.begin(), nodeNameLower.end(), nodeNameLower.begin(), ::tolower);
    if ("body" == nodeNameLower || "head" == nodeNameLower || "html" == nodeNameLower) {
        return new Step(node->nodeName().Utf8(), true);
    }

    std::string nodeName = node->nodeName().Utf8();
    if (!id.empty()) {
        return new Step(nodeName + idSelector(id), true);
    }
    ContainerNode* parent = node->parentNode();
    if (!parent || parent->IsDocumentNode()) {
        return new Step(nodeName, true);
    }


    std::vector<std::string> prefixedOwnClassNamesArray = prefixedElementClassNames(node);
    bool needsClassNames = false;
    bool needsNthChild = false;
    int ownIndex = -1;
    int elementIndex = -1;
    HTMLCollection* siblings = parent->Children();

    for (unsigned i = 0; !siblings->IsEmpty() && (ownIndex == -1 || !needsNthChild) && i < siblings->length(); ++i) {
        Element* sibling = siblings->item(i);
        elementIndex += 1;
        if (sibling == node) {
            ownIndex = elementIndex;
            continue;
        }

        if (needsNthChild) {
            continue;
        }
        if (sibling->nodeName().Utf8() != nodeName) {
            continue;
        }
        needsClassNames = true;
        set<string> ownClassNames(prefixedOwnClassNamesArray.begin(), prefixedOwnClassNamesArray.end());
        if (ownClassNames.empty()) {
            needsNthChild = true;
            continue;
        }

        std::vector<std::string> siblingClassNamesArray = prefixedElementClassNames(sibling);
        for (const auto& siblingClass : siblingClassNamesArray) {
            auto it = ownClassNames.find(siblingClass);
            if (it == ownClassNames.end()) {
                continue;
            }
            ownClassNames.erase(it);
            if (ownClassNames.empty()) {
                needsNthChild = true;
                break;
            }
        }
    }

    std::string result = nodeName;
    if (isTargetNode && "input" == nodeNameLower
        && !node->getAttribute(html_names::kTypeAttr).Utf8().empty()
        && node->getAttribute(html_names::kIdAttr).Utf8().empty()
        && node->getAttribute(html_names::kClassAttr).Utf8().empty()) {
        result += "[type=" + DOMWindowCSS::escape(node->getAttribute(html_names::kTypeAttr).GetString()).Utf8() + "]";
    }
    if (needsNthChild) {
        result += ":nth-child(" + to_string((ownIndex + 1)) + ")";
    } else if (needsClassNames) {
        for (const std::string& prefixedName : prefixedOwnClassNamesArray) {
            String name(prefixedName.substr(1));
            result += "." + DOMWindowCSS::escape(name).Utf8();
        }
    }
    return new DomPathUtils::Step(result, false);
}

}