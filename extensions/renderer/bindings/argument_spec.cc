// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/argument_spec.h"

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "gin/converter.h"
#include "gin/data_object_builder.h"
#include "gin/dictionary.h"

namespace extensions {

namespace {

// Returns a type string for the given |value|.
const char* GetV8ValueTypeString(v8::Local<v8::Value> value) {
  DCHECK(!value.IsEmpty());

  if (value->IsNull())
    return api_errors::kTypeNull;
  if (value->IsUndefined())
    return api_errors::kTypeUndefined;
  if (value->IsInt32())
    return api_errors::kTypeInteger;
  if (value->IsNumber())
    return api_errors::kTypeDouble;
  if (value->IsBoolean())
    return api_errors::kTypeBoolean;
  if (value->IsString())
    return api_errors::kTypeString;

  // Note: check IsArray(), IsFunction(), and IsArrayBuffer[View]() before
  // IsObject() since arrays, functions, and array buffers are objects.
  if (value->IsArray())
    return api_errors::kTypeList;
  if (value->IsFunction())
    return api_errors::kTypeFunction;
  if (value->IsArrayBuffer() || value->IsArrayBufferView())
    return api_errors::kTypeBinary;
  if (value->IsObject())
    return api_errors::kTypeObject;

  // TODO(devlin): The list above isn't exhaustive (it's missing at least
  // Symbol and Uint32). We may want to include those, since saying
  // "expected int, found other" isn't super helpful. On the other hand, authors
  // should be able to see what they passed.
  return "other";
}

// Returns true if |value| is within the bounds specified by |minimum| and
// |maximum|, populating |error| otherwise.
template <class T>
bool CheckFundamentalBounds(T value,
                            const base::Optional<int>& minimum,
                            const base::Optional<int>& maximum,
                            std::string* error) {
  if (minimum && value < *minimum) {
    *error = api_errors::NumberTooSmall(*minimum);
    return false;
  }
  if (maximum && value > *maximum) {
    *error = api_errors::NumberTooLarge(*maximum);
    return false;
  }
  return true;
}

}  // namespace

ArgumentSpec::ArgumentSpec(const base::Value& value) {
  const base::DictionaryValue* dict = nullptr;
  CHECK(value.GetAsDictionary(&dict));
  dict->GetBoolean("optional", &optional_);
  dict->GetString("name", &name_);

  InitializeType(dict);
}

ArgumentSpec::ArgumentSpec(ArgumentType type) : type_(type) {}

void ArgumentSpec::InitializeType(const base::DictionaryValue* dict) {
  std::string ref_string;
  if (dict->GetString("$ref", &ref_string)) {
    ref_ = std::move(ref_string);
    type_ = ArgumentType::REF;
    return;
  }

  {
    const base::ListValue* choices = nullptr;
    if (dict->GetList("choices", &choices)) {
      DCHECK(!choices->empty());
      type_ = ArgumentType::CHOICES;
      choices_.reserve(choices->GetSize());
      for (const auto& choice : *choices)
        choices_.push_back(std::make_unique<ArgumentSpec>(choice));
      return;
    }
  }

  std::string type_string;
  CHECK(dict->GetString("type", &type_string));
  if (type_string == "integer")
    type_ = ArgumentType::INTEGER;
  else if (type_string == "number")
    type_ = ArgumentType::DOUBLE;
  else if (type_string == "object")
    type_ = ArgumentType::OBJECT;
  else if (type_string == "array")
    type_ = ArgumentType::LIST;
  else if (type_string == "boolean")
    type_ = ArgumentType::BOOLEAN;
  else if (type_string == "string")
    type_ = ArgumentType::STRING;
  else if (type_string == "binary")
    type_ = ArgumentType::BINARY;
  else if (type_string == "any")
    type_ = ArgumentType::ANY;
  else if (type_string == "function")
    type_ = ArgumentType::FUNCTION;
  else
    NOTREACHED();

  int min = 0;
  if (dict->GetInteger("minimum", &min))
    minimum_ = min;

  int max = 0;
  if (dict->GetInteger("maximum", &max))
    maximum_ = max;

  int min_length = 0;
  if (dict->GetInteger("minLength", &min_length) ||
      dict->GetInteger("minItems", &min_length)) {
    DCHECK_GE(min_length, 0);
    min_length_ = min_length;
  }

  int max_length = 0;
  if (dict->GetInteger("maxLength", &max_length) ||
      dict->GetInteger("maxItems", &max_length)) {
    DCHECK_GE(max_length, 0);
    max_length_ = max_length;
  }

  if (type_ == ArgumentType::OBJECT) {
    const base::DictionaryValue* properties_value = nullptr;
    if (dict->GetDictionary("properties", &properties_value)) {
      for (base::DictionaryValue::Iterator iter(*properties_value);
           !iter.IsAtEnd(); iter.Advance()) {
        properties_[iter.key()] = std::make_unique<ArgumentSpec>(iter.value());
      }
    }
    const base::DictionaryValue* additional_properties_value = nullptr;
    if (dict->GetDictionary("additionalProperties",
                            &additional_properties_value)) {
      additional_properties_ =
          std::make_unique<ArgumentSpec>(*additional_properties_value);
      // Additional properties are always optional.
      additional_properties_->optional_ = true;
    }
  } else if (type_ == ArgumentType::LIST) {
    const base::DictionaryValue* item_value = nullptr;
    CHECK(dict->GetDictionary("items", &item_value));
    list_element_type_ = std::make_unique<ArgumentSpec>(*item_value);
  } else if (type_ == ArgumentType::STRING) {
    // Technically, there's no reason enums couldn't be other objects (e.g.
    // numbers), but right now they seem to be exclusively strings. We could
    // always update this if need be.
    const base::ListValue* enums = nullptr;
    if (dict->GetList("enum", &enums)) {
      size_t size = enums->GetSize();
      CHECK_GT(size, 0u);
      for (size_t i = 0; i < size; ++i) {
        std::string enum_value;
        // Enum entries come in two versions: a list of possible strings, and
        // a dictionary with a field 'name'.
        if (!enums->GetString(i, &enum_value)) {
          const base::DictionaryValue* enum_value_dictionary = nullptr;
          CHECK(enums->GetDictionary(i, &enum_value_dictionary));
          CHECK(enum_value_dictionary->GetString("name", &enum_value));
        }
        enum_values_.insert(std::move(enum_value));
      }
    }
  }

  // Check if we should preserve null in objects. Right now, this is only used
  // on arguments of type object and any (in fact, it's only used in the storage
  // API), but it could potentially make sense for lists or functions as well.
  if (type_ == ArgumentType::OBJECT || type_ == ArgumentType::ANY)
    dict->GetBoolean("preserveNull", &preserve_null_);

  if (type_ == ArgumentType::OBJECT || type_ == ArgumentType::BINARY) {
    std::string instance_of;
    if (dict->GetString("isInstanceOf", &instance_of))
      instance_of_ = instance_of;
  }
}

ArgumentSpec::~ArgumentSpec() {}

bool ArgumentSpec::IsCorrectType(v8::Local<v8::Value> value,
                                 const APITypeReferenceMap& refs,
                                 std::string* error) const {
  bool is_valid_type = false;

  switch (type_) {
    case ArgumentType::INTEGER:
      // -0 is treated internally as a double, but we classify it as an integer.
      is_valid_type =
          value->IsInt32() ||
          (value->IsNumber() && value.As<v8::Number>()->Value() == 0.0);
      break;
    case ArgumentType::DOUBLE:
      is_valid_type = value->IsNumber();
      break;
    case ArgumentType::BOOLEAN:
      is_valid_type = value->IsBoolean();
      break;
    case ArgumentType::STRING:
      is_valid_type = value->IsString();
      break;
    case ArgumentType::OBJECT:
      // Don't allow functions or arrays (even though they are technically
      // objects). This is to make it easier to match otherwise-ambiguous
      // signatures. For instance, if an API method has an optional object
      // parameter and then an optional callback, we wouldn't necessarily be
      // able to match the arguments if we allowed functions as objects.
      // TODO(devlin): What about other subclasses of Object, like Map and Set?
      is_valid_type =
          value->IsObject() && !value->IsFunction() && !value->IsArray();
      break;
    case ArgumentType::LIST:
      is_valid_type = value->IsArray();
      break;
    case ArgumentType::BINARY:
      is_valid_type = value->IsArrayBuffer() || value->IsArrayBufferView();
      break;
    case ArgumentType::FUNCTION:
      is_valid_type = value->IsFunction();
      break;
    case ArgumentType::ANY:
      is_valid_type = true;
      break;
    case ArgumentType::REF: {
      DCHECK(ref_);
      const ArgumentSpec* reference = refs.GetSpec(ref_.value());
      DCHECK(reference) << ref_.value();
      is_valid_type = reference->IsCorrectType(value, refs, error);
      break;
    }
    case ArgumentType::CHOICES:
      for (const auto& choice : choices_) {
        if (choice->IsCorrectType(value, refs, error)) {
          is_valid_type = true;
          break;
        }
      }
      break;
  }

  if (!is_valid_type)
    *error = GetInvalidTypeError(value);
  return is_valid_type;
}

bool ArgumentSpec::ParseArgument(v8::Local<v8::Context> context,
                                 v8::Local<v8::Value> value,
                                 const APITypeReferenceMap& refs,
                                 std::unique_ptr<base::Value>* out_value,
                                 v8::Local<v8::Value>* v8_out_value,
                                 std::string* error) const {
  // Note: for top-level arguments (i.e., those passed directly to the function,
  // as opposed to a property on an object, or the item of an array), we will
  // have already checked the type. Doing so again should be nearly free, but
  // if we do find this to be an issue, we could avoid the second call.
  if (!IsCorrectType(value, refs, error))
    return false;

  switch (type_) {
    case ArgumentType::INTEGER:
    case ArgumentType::DOUBLE:
    case ArgumentType::BOOLEAN:
    case ArgumentType::STRING:
      return ParseArgumentToFundamental(context, value, out_value, v8_out_value,
                                        error);
    case ArgumentType::OBJECT:
      return ParseArgumentToObject(context, value.As<v8::Object>(), refs,
                                   out_value, v8_out_value, error);
    case ArgumentType::LIST:
      return ParseArgumentToArray(context, value.As<v8::Array>(), refs,
                                  out_value, v8_out_value, error);
    case ArgumentType::BINARY:
      return ParseArgumentToAny(context, value, out_value, v8_out_value, error);
    case ArgumentType::FUNCTION:
      if (out_value) {
        // Certain APIs (contextMenus) have functions as parameters other than
        // the callback (contextMenus uses it for an onclick listener). Our
        // generated types have adapted to consider functions "objects" and
        // serialize them as dictionaries.
        // TODO(devlin): It'd be awfully nice to get rid of this eccentricity.
        *out_value = std::make_unique<base::DictionaryValue>();
      }

      if (v8_out_value)
        *v8_out_value = value;

      return true;
    case ArgumentType::REF: {
      DCHECK(ref_);
      const ArgumentSpec* reference = refs.GetSpec(ref_.value());
      DCHECK(reference) << ref_.value();
      return reference->ParseArgument(context, value, refs, out_value,
                                      v8_out_value, error);
    }
    case ArgumentType::CHOICES: {
      for (const auto& choice : choices_) {
        if (choice->ParseArgument(context, value, refs, out_value, v8_out_value,
                                  error)) {
          return true;
        }
      }
      *error = api_errors::InvalidChoice();
      return false;
    }
    case ArgumentType::ANY:
      return ParseArgumentToAny(context, value, out_value, v8_out_value, error);
  }

  NOTREACHED();
  return false;
}

const std::string& ArgumentSpec::GetTypeName() const {
  if (!type_name_.empty())
    return type_name_;

  switch (type_) {
    case ArgumentType::INTEGER:
      type_name_ = api_errors::kTypeInteger;
      break;
    case ArgumentType::DOUBLE:
      type_name_ = api_errors::kTypeDouble;
      break;
    case ArgumentType::BOOLEAN:
      type_name_ = api_errors::kTypeBoolean;
      break;
    case ArgumentType::STRING:
      type_name_ = api_errors::kTypeString;
      break;
    case ArgumentType::OBJECT:
      type_name_ = instance_of_ ? *instance_of_ : api_errors::kTypeObject;
      break;
    case ArgumentType::LIST:
      type_name_ = api_errors::kTypeList;
      break;
    case ArgumentType::BINARY:
      type_name_ = api_errors::kTypeBinary;
      break;
    case ArgumentType::FUNCTION:
      type_name_ = api_errors::kTypeFunction;
      break;
    case ArgumentType::REF:
      type_name_ = ref_->c_str();
      break;
    case ArgumentType::CHOICES: {
      std::vector<base::StringPiece> choices_strings;
      choices_strings.reserve(choices_.size());
      for (const auto& choice : choices_)
        choices_strings.push_back(choice->GetTypeName());
      type_name_ = base::StringPrintf(
          "[%s]", base::JoinString(choices_strings, "|").c_str());
      break;
    }
    case ArgumentType::ANY:
      type_name_ = api_errors::kTypeAny;
      break;
  }
  DCHECK(!type_name_.empty());
  return type_name_;
}

bool ArgumentSpec::ParseArgumentToFundamental(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value,
    std::unique_ptr<base::Value>* out_value,
    v8::Local<v8::Value>* v8_out_value,
    std::string* error) const {
  switch (type_) {
    case ArgumentType::INTEGER: {
      DCHECK(value->IsNumber());
      int int_val = 0;
      if (value->IsInt32()) {
        int_val = value.As<v8::Int32>()->Value();
      } else {
        // See comment in IsCorrectType().
        DCHECK_EQ(0.0, value.As<v8::Number>()->Value());
        int_val = 0;
      }
      if (!CheckFundamentalBounds(int_val, minimum_, maximum_, error))
        return false;
      if (out_value)
        *out_value = std::make_unique<base::Value>(int_val);
      if (v8_out_value)
        *v8_out_value = v8::Integer::New(context->GetIsolate(), int_val);
      return true;
    }
    case ArgumentType::DOUBLE: {
      DCHECK(value->IsNumber());
      double double_val = value.As<v8::Number>()->Value();
      if (!CheckFundamentalBounds(double_val, minimum_, maximum_, error))
        return false;
      if (out_value)
        *out_value = std::make_unique<base::Value>(double_val);
      if (v8_out_value)
        *v8_out_value = value;
      return true;
    }
    case ArgumentType::STRING: {
      DCHECK(value->IsString());

      v8::Local<v8::String> v8_string = value.As<v8::String>();
      size_t length = static_cast<size_t>(v8_string->Length());
      if (min_length_ && length < *min_length_) {
        *error = api_errors::TooFewStringChars(*min_length_, length);
        return false;
      }

      if (max_length_ && length > *max_length_) {
        *error = api_errors::TooManyStringChars(*max_length_, length);
        return false;
      }

      if (!enum_values_.empty() || out_value) {
        std::string str;
        // We already checked that this is a string, so this should never fail.
        CHECK(gin::Converter<std::string>::FromV8(context->GetIsolate(), value,
                                                  &str));
        if (!enum_values_.empty() && enum_values_.count(str) == 0) {
          *error = api_errors::InvalidEnumValue(enum_values_);
          return false;
        }

        if (out_value)
          *out_value = std::make_unique<base::Value>(std::move(str));
      }

      if (v8_out_value)
        *v8_out_value = value;

      return true;
    }
    case ArgumentType::BOOLEAN: {
      DCHECK(value->IsBoolean());
      if (out_value) {
        *out_value =
            std::make_unique<base::Value>(value.As<v8::Boolean>()->Value());
      }
      if (v8_out_value)
        *v8_out_value = value;

      return true;
    }
    default:
      NOTREACHED();
  }
  return false;
}

bool ArgumentSpec::ParseArgumentToObject(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object,
    const APITypeReferenceMap& refs,
    std::unique_ptr<base::Value>* out_value,
    v8::Local<v8::Value>* v8_out_value,
    std::string* error) const {
  DCHECK_EQ(ArgumentType::OBJECT, type_);
  std::unique_ptr<base::DictionaryValue> result;
  // Only construct the result if we have an |out_value| to populate.
  if (out_value)
    result = std::make_unique<base::DictionaryValue>();

  // We don't convert to a new object in two cases:
  // - If instanceof is specified, we don't want to create a new data object,
  //   because then the object wouldn't be an instanceof the specified type.
  //   e.g., if a function is expecting a RegExp, we need to make sure the
  //   value passed in is, indeed, a RegExp, which won't be the case if we just
  //   copy the properties to a new object.
  // - Some methods use additional_properties_ in order to allow for arbitrary
  //   types to be passed in (e.g., test.assertThrows allows a "self" property
  //   to be provided). Similar to above, if we just copy the property values,
  //   it may change the type of the object and break expectations.
  // TODO(devlin): The latter case could be handled by specifying a different
  // tag to indicate that we don't want to convert. This would be much clearer,
  // and allow us to handle the other additional_properties_ cases. But first,
  // we need to track down all the instances that use it.
  bool convert_to_v8 = v8_out_value && !additional_properties_ && !instance_of_;
  gin::DataObjectBuilder v8_result(context->GetIsolate());

  v8::Local<v8::Array> own_property_names;
  if (!object->GetOwnPropertyNames(context).ToLocal(&own_property_names)) {
    *error = api_errors::ScriptThrewError();
    return false;
  }

  // Track all properties we see from |properties_| to check if any are missing.
  // Use ArgumentSpec* instead of std::string for comparison + copy efficiency.
  std::set<const ArgumentSpec*> seen_properties;
  uint32_t length = own_property_names->Length();
  std::string property_error;
  for (uint32_t i = 0; i < length; ++i) {
    v8::Local<v8::Value> key;
    if (!own_property_names->Get(context, i).ToLocal(&key)) {
      *error = api_errors::ScriptThrewError();
      return false;
    }
    // In JS, all keys are strings or numbers (or symbols, but those are
    // excluded by GetOwnPropertyNames()). If you try to set anything else
    // (e.g. an object), it is converted to a string.
    DCHECK(key->IsString() || key->IsNumber());
    v8::String::Utf8Value utf8_key(context->GetIsolate(), key);

    ArgumentSpec* property_spec = nullptr;
    auto iter = properties_.find(*utf8_key);
    bool allow_unserializable = false;
    if (iter != properties_.end()) {
      property_spec = iter->second.get();
      seen_properties.insert(property_spec);
    } else if (additional_properties_) {
      property_spec = additional_properties_.get();
      // additionalProperties: {type: any} is often used to allow anything
      // through, including things that would normally break serialization like
      // functions, or even NaN. If the additional properties are of
      // ArgumentType::ANY, allow anything, even if it doesn't serialize.
      allow_unserializable = property_spec->type_ == ArgumentType::ANY;
    } else {
      *error = api_errors::UnexpectedProperty(*utf8_key);
      return false;
    }

    v8::Local<v8::Value> prop_value;
    // Fun: It's possible that a previous getter has removed the property from
    // the object. This isn't that big of a deal, since it would only manifest
    // in the case of some reasonably-crazy script objects, and it's probably
    // not worth optimizing for the uncommon case to the detriment of the
    // common (and either should be totally safe). We can always add a
    // HasOwnProperty() check here in the future, if we desire.
    // See also comment in ParseArgumentToArray() about passing in custom
    // crazy values here.
    if (!object->Get(context, key).ToLocal(&prop_value)) {
      *error = api_errors::ScriptThrewError();
      return false;
    }

    // Note: We don't serialize undefined, and only serialize null if it's part
    // of the spec.
    // TODO(devlin): This matches current behavior, but it is correct? And
    // we treat undefined and null the same?
    if (prop_value->IsUndefined() || prop_value->IsNull()) {
      if (!property_spec->optional_) {
        *error = api_errors::MissingRequiredProperty(*utf8_key);
        return false;
      }
      if (preserve_null_ && prop_value->IsNull()) {
        if (result) {
          result->SetWithoutPathExpansion(*utf8_key,
                                          std::make_unique<base::Value>());
        }
        if (convert_to_v8)
          v8_result.Set(*utf8_key, prop_value);
      }
      continue;
    }

    std::unique_ptr<base::Value> property;
    v8::Local<v8::Value> v8_property;
    if (!property_spec->ParseArgument(
            context, prop_value, refs, out_value ? &property : nullptr,
            convert_to_v8 ? &v8_property : nullptr, &property_error)) {
      if (allow_unserializable)
        continue;
      *error = api_errors::PropertyError(*utf8_key, property_error);
      return false;
    }
    if (out_value)
      result->SetWithoutPathExpansion(*utf8_key, std::move(property));
    if (convert_to_v8)
      v8_result.Set(*utf8_key, v8_property);
  }

  for (const auto& pair : properties_) {
    const ArgumentSpec* spec = pair.second.get();
    if (!spec->optional_ && seen_properties.count(spec) == 0) {
      *error = api_errors::MissingRequiredProperty(pair.first.c_str());
      return false;
    }
  }

  if (instance_of_) {
    // Check for the instance somewhere in the object's prototype chain.
    // NOTE: This only checks that something in the prototype chain was
    // constructed with the same name as the desired instance, but doesn't
    // validate that it's the same constructor as the expected one. For
    // instance, if we expect isInstanceOf == 'Date', script could pass in
    // (function() {
    //   function Date() {}
    //   return new Date();
    // })()
    // Since the object contains 'Date' in its prototype chain, this check
    // succeeds, even though the object is not of built-in type Date.
    // Since this isn't (or at least shouldn't be) a security check, this is
    // okay.
    bool found = false;
    v8::Local<v8::Value> next_check = object;
    do {
      v8::Local<v8::Object> current = next_check.As<v8::Object>();
      v8::String::Utf8Value constructor(context->GetIsolate(),
                                        current->GetConstructorName());
      if (*instance_of_ ==
          base::StringPiece(*constructor, constructor.length())) {
        found = true;
        break;
      }
      next_check = current->GetPrototype();
    } while (next_check->IsObject());

    if (!found) {
      *error = api_errors::NotAnInstance(instance_of_->c_str());
      return false;
    }
  }

  if (out_value)
    *out_value = std::move(result);

  if (v8_out_value) {
    if (convert_to_v8) {
      v8::Local<v8::Object> converted = v8_result.Build();
      // We set the object's prototype to Null() so that handlers avoid
      // triggering any tricky getters or setters on Object.prototype.
      CHECK(converted->SetPrototype(context, v8::Null(context->GetIsolate()))
                .ToChecked());
      *v8_out_value = converted;
    } else {
      *v8_out_value = object;
    }
  }

  return true;
}

bool ArgumentSpec::ParseArgumentToArray(v8::Local<v8::Context> context,
                                        v8::Local<v8::Array> value,
                                        const APITypeReferenceMap& refs,
                                        std::unique_ptr<base::Value>* out_value,
                                        v8::Local<v8::Value>* v8_out_value,
                                        std::string* error) const {
  DCHECK_EQ(ArgumentType::LIST, type_);

  uint32_t length = value->Length();
  if (min_length_ && length < *min_length_) {
    *error = api_errors::TooFewArrayItems(*min_length_, length);
    return false;
  }

  if (max_length_ && length > *max_length_) {
    *error = api_errors::TooManyArrayItems(*max_length_, length);
    return false;
  }

  std::unique_ptr<base::ListValue> result;
  // Only construct the result if we have an |out_value| to populate.
  if (out_value)
    result = std::make_unique<base::ListValue>();
  v8::Local<v8::Array> v8_result;
  if (v8_out_value)
    v8_result = v8::Array::New(context->GetIsolate(), length);

  std::string item_error;
  for (uint32_t i = 0; i < length; ++i) {
    v8::MaybeLocal<v8::Value> maybe_subvalue = value->Get(context, i);
    v8::Local<v8::Value> subvalue;
    // Note: This can fail in the case of a developer passing in the following:
    // var a = [];
    // Object.defineProperty(a, 0, { get: () => { throw new Error('foo'); } });
    // Currently, this will cause the developer-specified error ('foo') to be
    // thrown.
    // TODO(devlin): This is probably fine, but it's worth contemplating
    // catching the error and throwing our own.
    if (!maybe_subvalue.ToLocal(&subvalue))
      return false;
    std::unique_ptr<base::Value> item;
    v8::Local<v8::Value> v8_item;
    if (!list_element_type_->ParseArgument(
            context, subvalue, refs, out_value ? &item : nullptr,
            v8_out_value ? &v8_item : nullptr, &item_error)) {
      *error = api_errors::IndexError(i, item_error);
      return false;
    }
    if (out_value)
      result->Append(std::move(item));
    if (v8_out_value) {
      // This should never fail, since it's a newly-created array with
      // CreateDataProperty().
      CHECK(v8_result->CreateDataProperty(context, i, v8_item).ToChecked());
    }
  }

  if (out_value)
    *out_value = std::move(result);
  if (v8_out_value)
    *v8_out_value = v8_result;

  return true;
}

bool ArgumentSpec::ParseArgumentToAny(v8::Local<v8::Context> context,
                                      v8::Local<v8::Value> value,
                                      std::unique_ptr<base::Value>* out_value,
                                      v8::Local<v8::Value>* v8_out_value,
                                      std::string* error) const {
  DCHECK(type_ == ArgumentType::ANY || type_ == ArgumentType::BINARY);
  if (out_value) {
    std::unique_ptr<content::V8ValueConverter> converter =
        content::V8ValueConverter::Create();
    converter->SetStripNullFromObjects(!preserve_null_);
    converter->SetConvertNegativeZeroToInt(true);
    // Note: don't allow functions. Functions are handled either by the specific
    // type (ArgumentType::FUNCTION) or by allowing arbitrary optional
    // arguments, which allows unserializable values.
    // TODO(devlin): Is this correct? Or do we rely on an 'any' type of function
    // being serialized in an odd-ball API?
    std::unique_ptr<base::Value> converted =
        converter->FromV8Value(value, context);
    if (!converted) {
      *error = api_errors::UnserializableValue();
      return false;
    }
    if (type_ == ArgumentType::BINARY)
      DCHECK_EQ(base::Value::Type::BINARY, converted->type());
    *out_value = std::move(converted);
  }
  if (v8_out_value)
    *v8_out_value = value;

  return true;
}

std::string ArgumentSpec::GetInvalidTypeError(
    v8::Local<v8::Value> value) const {
  return api_errors::InvalidType(GetTypeName().c_str(),
                                 GetV8ValueTypeString(value));
}

}  // namespace extensions
