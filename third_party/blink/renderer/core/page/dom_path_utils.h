//
// Created by zpj on 2/7/23.
//

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_DOM_PATH_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_DOM_PATH_UTILS_H_

#include <utility>

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

class CORE_EXPORT DomPathUtils {
public:
    struct Step {
        std::string value;
        bool optimized;
        Step(std::string value, bool optimized) {
            this->value = std::move(value);
            this->optimized = optimized;
        }
    };
    static std::string GetCssSelector(Node* node, bool optimized);
private:
    static std::vector<std::string> prefixedElementClassNames(Element* node);
    static std::string idSelector(std::string& id);
    static Step* cssPathStep(Element* node, bool optimized, bool isTargetNode);
};

}  // namespace blink

#endif // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_DOM_PATH_UTILS_H_
