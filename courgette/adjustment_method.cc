// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/adjustment_method.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"

namespace courgette {

////////////////////////////////////////////////////////////////////////////////

class NullAdjustmentMethod : public AdjustmentMethod {
  bool Adjust(const AssemblyProgram& model, AssemblyProgram* program) {
    return true;
  }
};

////////////////////////////////////////////////////////////////////////////////

// The purpose of adjustment is to assign indexes to Labels of a program 'p' to
// make the sequence of indexes similar to a 'model' program 'm'.  Labels
// themselves don't have enough information to do this job, so we work with a
// LabelInfo surrogate for each label.
//
class LabelInfo {
 public:
  Label* label_;              // The label that this info a surrogate for.

  // Information used only in debugging messages.
  uint32_t is_model_ : 1;      // Is the label in the model?
  uint32_t debug_index_ : 31;  // An unique small number for naming the label.

  uint32_t refs_;  // Number of times this Label is referenced.

  LabelInfo* assignment_;     // Label from other program corresponding to this.

  // LabelInfos are in a doubly linked list ordered by address (label_->rva_) so
  // we can quickly find Labels adjacent in address order.
  LabelInfo* next_addr_;      // Label(Info) at next highest address.
  LabelInfo* prev_addr_;      // Label(Info) at next lowest address.

  std::vector<uint32_t> positions_;  // Offsets into the trace of references.

  // Just a no-argument constructor and copy constructor.  Actual LabelInfo
  // objects are allocated in std::pair structs in a std::map.
  LabelInfo()
      : label_(nullptr),
        is_model_(false),
        debug_index_(0),
        refs_(0),
        assignment_(nullptr),
        next_addr_(nullptr),
        prev_addr_(nullptr) {}

 private:
  void operator=(const LabelInfo*);  // Disallow assignment only.

  // Public compiler generated copy constructor is needed to constuct
  // std::pair<Label*, LabelInfo> so that fresh LabelInfos can be allocated
  // inside a std::map.
};

struct OrderLabelInfoByAddressAscending {
  bool operator()(const LabelInfo* a, const LabelInfo* b) const {
    return a->label_->rva_ < b->label_->rva_;
  }
};

static std::string ToString(LabelInfo* info) {
  std::string s;
  base::StringAppendF(&s, "%c%d", "pm"[info->is_model_], info->debug_index_);
  if (info->label_->index_ != Label::kNoIndex)
    base::StringAppendF(&s, " (%d)", info->label_->index_);

  base::StringAppendF(&s, " #%u", info->refs_);
  return s;
}

// General graph matching is exponential, essentially trying all permutations.
// The exponential algorithm can be made faster by avoiding consideration of
// impossible or unlikely matches.  We can make the matching practical by eager
// matching - by looking for likely matches and commiting to them, and using the
// committed assignment as the basis for further matching.
//
// The basic eager graph-matching assignment is based on several ideas:
//
//  * The strongest match will be for parts of the program that have not
//    changed.  If part of a program has not changed, then the number of
//    references to a label will be the same, and corresponding pairs of
//    adjacent labels will have the same RVA difference.
//
//  * Some assignments are 'obvious' if you look at the distribution.  Example:
//    if both the program and the model have a label that is referred to much
//    more often than the next most refered-to label, it is likely the two
//    labels correspond.
//
//  * If a label from the program corresponds to a label in the model, it is
//    likely that the labels near the corresponding labels also match.  A
//    conservative way of extending the match is to assign only those labels
//    which have exactly the same address offset and reference count.
//
//  * If two labels correspond, then we can try to match up the references
//    before and after the labels in the reference stream.  For this to be
//    practical, the number of references has to be small, e.g. each label has
//    exactly one reference.
//

// Note: we also tried a completely different approach: random assignment
// followed by simulated annealing.  This produced similar results.  The results
// were not as good for very small differences because the simulated annealing
// never quite hit the groove.  And simulated annealing was several orders of
// magnitude slower.


// TRIE node for suffix strings in the label reference sequence.
//
// We dynamically build a trie for both the program and model, growing the trie
// as necessary.  The trie node for a (possibly) empty string of label
// references contains the distribution of labels following the string.  The
// roots node (for the empty string) thus contains the simple distribution of
// labels within the label reference stream.

struct Node {
  Node(LabelInfo* in_edge, Node* prev)
      : in_edge_(in_edge), prev_(prev), count_(0),
        in_queue_(false) {
    length_ = 1 + (prev_ ? prev_->length_ : 0);
  }
  LabelInfo* in_edge_;  //
  Node* prev_;          // Node at shorter length.
  int count_;           // Frequency of this path in Trie.
  int length_;
  typedef std::map<LabelInfo*, Node*> Edges;
  Edges edges_;
  std::vector<int> places_;   // Indexes into sequence of this item.
  std::list<Node*> edges_in_frequency_order;

  bool in_queue_;
  bool Extended() const { return !edges_.empty(); }

  uint32_t Weight() const { return edges_in_frequency_order.front()->count_; }
};

static std::string ToString(Node* node) {
  std::vector<std::string> prefix;
  for (Node* n = node;  n->prev_;  n = n->prev_)
    prefix.push_back(ToString(n->in_edge_));

  std::string s;
  s += "{";
  const char* sep = "";
  while (!prefix.empty()) {
    s += sep;
    sep = ",";
    s += prefix.back();
    prefix.pop_back();
  }

  s += base::StringPrintf("%u", node->count_);
  s += " @";
  s += base::NumberToString(node->edges_in_frequency_order.size());
  s += "}";
  return s;
}

typedef std::vector<LabelInfo*> Trace;

struct OrderNodeByCountDecreasing {
  bool operator()(Node* a, Node* b) const {
    if (a->count_ != b->count_)
      return  (a->count_) > (b->count_);
    return a->places_.at(0) < b->places_.at(0);  // Prefer first occuring.
  }
};

struct OrderNodeByWeightDecreasing {
  bool operator()(Node* a, Node* b) const {
    // (Maybe tie-break on total count, followed by lowest assigned node indexes
    // in path.)
    uint32_t a_weight = a->Weight();
    uint32_t b_weight = b->Weight();
    if (a_weight != b_weight)
      return a_weight > b_weight;
    if (a->length_ != b->length_)
      return a->length_ > b->length_;            // Prefer longer.
    return a->places_.at(0) < b->places_.at(0);  // Prefer first occuring.
  }
};

typedef std::set<Node*, OrderNodeByWeightDecreasing> NodeQueue;

class AssignmentProblem {
 public:
  AssignmentProblem(const Trace& model, const Trace& problem)
      : m_trace_(model),
        p_trace_(problem),
        m_root_(nullptr),
        p_root_(nullptr) {}

  ~AssignmentProblem() {
    for (size_t i = 0;  i < all_nodes_.size();  ++i)
      delete all_nodes_[i];
  }

  bool Solve() {
    m_root_ = MakeRootNode(m_trace_);
    p_root_ = MakeRootNode(p_trace_);
    AddToQueue(p_root_);

    while (!worklist_.empty()) {
      Node* node = *worklist_.begin();
      node->in_queue_ = false;
      worklist_.erase(node);
      TrySolveNode(node);
    }

    VLOG(2) << unsolved_.size() << " unsolved items";
    return true;
  }

 private:
  void AddToQueue(Node* node) {
    if (node->length_ >= 10) {
      VLOG(4) << "Length clipped " << ToString(node->prev_);
      return;
    }
    if (node->in_queue_) {
      LOG(ERROR) << "Double add " << ToString(node);
      return;
    }
    // just to be sure data for prioritizing is available
    ExtendNode(node, p_trace_);
    // SkipCommittedLabels(node);
    if (node->edges_in_frequency_order.empty())
      return;
    node->in_queue_ = true;
    worklist_.insert(node);
  }

  void SkipCommittedLabels(Node* node) {
    ExtendNode(node, p_trace_);
    uint32_t skipped = 0;
    while (!node->edges_in_frequency_order.empty() &&
           node->edges_in_frequency_order.front()->in_edge_->assignment_) {
      ++skipped;
      node->edges_in_frequency_order.pop_front();
    }
    if (skipped > 0)
      VLOG(4) << "Skipped " << skipped << " at " << ToString(node);
  }

  void TrySolveNode(Node* p_node) {
    Node* front = p_node->edges_in_frequency_order.front();
    if (front->in_edge_->assignment_) {
      p_node->edges_in_frequency_order.pop_front();
      AddToQueue(front);
      AddToQueue(p_node);
      return;
    }

    // Compare frequencies of unassigned edges, and either make
    // assignment(s) or move node to unsolved list

    Node* m_node = FindModelNode(p_node);

    if (m_node == nullptr) {
      VLOG(2) << "Can't find model node";
      unsolved_.insert(p_node);
      return;
    }
    ExtendNode(m_node, m_trace_);

    // Lets just try greedy

    SkipCommittedLabels(m_node);
    if (m_node->edges_in_frequency_order.empty()) {
      VLOG(4) << "Punting, no elements left in model vs "
              << p_node->edges_in_frequency_order.size();
      unsolved_.insert(p_node);
      return;
    }
    Node* m_match = m_node->edges_in_frequency_order.front();
    Node* p_match = p_node->edges_in_frequency_order.front();

    if (p_match->count_ > 1.1 * m_match->count_  ||
        m_match->count_ > 1.1 * p_match->count_) {
      VLOG(3) << "Tricky distribution "
              << p_match->count_ << ":" << m_match->count_ << "  "
              << ToString(p_match) << " vs " << ToString(m_match);
      return;
    }

    m_node->edges_in_frequency_order.pop_front();
    p_node->edges_in_frequency_order.pop_front();

    LabelInfo* p_label_info = p_match->in_edge_;
    LabelInfo* m_label_info = m_match->in_edge_;
    int m_index = p_label_info->label_->index_;
    if (m_index != Label::kNoIndex) {
      VLOG(2) << "Cant use unassigned label from model " << m_index;
      unsolved_.insert(p_node);
      return;
    }

    Assign(p_label_info, m_label_info);

    AddToQueue(p_match);  // find matches within new match
    AddToQueue(p_node);   // and more matches within this node
  }

  void Assign(LabelInfo* p_info, LabelInfo* m_info) {
    AssignOne(p_info, m_info);
    VLOG(4) << "Assign " << ToString(p_info) << " := " << ToString(m_info);
    // Now consider unassigned adjacent addresses
    TryExtendAssignment(p_info, m_info);
  }

  void AssignOne(LabelInfo* p_info, LabelInfo* m_info) {
    p_info->label_->index_ = m_info->label_->index_;

    // Mark as assigned
    m_info->assignment_ = p_info;
    p_info->assignment_ = m_info;
  }

  void TryExtendAssignment(LabelInfo* p_info, LabelInfo* m_info) {
    RVA m_rva_base = m_info->label_->rva_;
    RVA p_rva_base = p_info->label_->rva_;

    LabelInfo* m_info_next = m_info->next_addr_;
    LabelInfo* p_info_next = p_info->next_addr_;
    for ( ; m_info_next && p_info_next; ) {
      if (m_info_next->assignment_)
        break;

      RVA m_rva = m_info_next->label_->rva_;
      RVA p_rva = p_info_next->label_->rva_;

      if (m_rva - m_rva_base != p_rva - p_rva_base) {
        // previous label was pointing to something that is different size
        break;
      }
      LabelInfo* m_info_next_next = m_info_next->next_addr_;
      LabelInfo* p_info_next_next = p_info_next->next_addr_;
      if (m_info_next_next && p_info_next_next) {
        RVA m_rva_next = m_info_next_next->label_->rva_;
        RVA p_rva_next = p_info_next_next->label_->rva_;
        if (m_rva_next - m_rva != p_rva_next - p_rva) {
          // Since following labels are no longer in address lockstep, assume
          // this address has a difference.
          break;
        }
      }

      // The label has inconsistent numbers of references, it is probably not
      // the same thing.
      if (m_info_next->refs_ != p_info_next->refs_) {
        break;
      }

      VLOG(4) << "  Extending assignment -> "
              << ToString(p_info_next) << " := " << ToString(m_info_next);

      AssignOne(p_info_next, m_info_next);

      if (p_info_next->refs_ == m_info_next->refs_ &&
          p_info_next->refs_ == 1) {
        TryExtendSequence(p_info_next->positions_[0],
                          m_info_next->positions_[0]);
        TryExtendSequenceBackwards(p_info_next->positions_[0],
                                   m_info_next->positions_[0]);
      }

      p_info_next = p_info_next_next;
      m_info_next = m_info_next_next;
    }

    LabelInfo* m_info_prev = m_info->prev_addr_;
    LabelInfo* p_info_prev = p_info->prev_addr_;
    for ( ; m_info_prev && p_info_prev; ) {
      if (m_info_prev->assignment_)
        break;

      RVA m_rva = m_info_prev->label_->rva_;
      RVA p_rva = p_info_prev->label_->rva_;

      if (m_rva - m_rva_base != p_rva - p_rva_base) {
        // previous label was pointing to something that is different size
        break;
      }
      LabelInfo* m_info_prev_prev = m_info_prev->prev_addr_;
      LabelInfo* p_info_prev_prev = p_info_prev->prev_addr_;

      // The the label has inconsistent numbers of references, it is
      // probably not the same thing
      if (m_info_prev->refs_ != p_info_prev->refs_) {
        break;
      }

      AssignOne(p_info_prev, m_info_prev);
      VLOG(4) << "  Extending assignment <- " << ToString(p_info_prev) << " := "
              << ToString(m_info_prev);

      p_info_prev = p_info_prev_prev;
      m_info_prev = m_info_prev_prev;
    }
  }

  uint32_t TryExtendSequence(uint32_t p_pos_start, uint32_t m_pos_start) {
    uint32_t p_pos = p_pos_start + 1;
    uint32_t m_pos = m_pos_start + 1;

    while (p_pos < p_trace_.size()  &&  m_pos < m_trace_.size()) {
      LabelInfo* p_info = p_trace_[p_pos];
      LabelInfo* m_info = m_trace_[m_pos];

      // To match, either (1) both are assigned or (2) both are unassigned.
      if ((p_info->assignment_ == nullptr) != (m_info->assignment_ == nullptr))
        break;

      // If they are assigned, it needs to be consistent (same index).
      if (p_info->assignment_ && m_info->assignment_) {
        if (p_info->label_->index_ != m_info->label_->index_)
          break;
        ++p_pos;
        ++m_pos;
        continue;
      }

      if (p_info->refs_ != m_info->refs_)
        break;

      AssignOne(p_info, m_info);
      VLOG(4) << "    Extending assignment seq[+" << p_pos - p_pos_start
              << "] -> " << ToString(p_info) << " := " << ToString(m_info);

      ++p_pos;
      ++m_pos;
    }

    return p_pos - p_pos_start;
  }

  uint32_t TryExtendSequenceBackwards(uint32_t p_pos_start,
                                      uint32_t m_pos_start) {
    if (p_pos_start == 0  ||  m_pos_start == 0)
      return 0;

    uint32_t p_pos = p_pos_start - 1;
    uint32_t m_pos = m_pos_start - 1;

    while (p_pos > 0  &&  m_pos > 0) {
      LabelInfo* p_info = p_trace_[p_pos];
      LabelInfo* m_info = m_trace_[m_pos];

      if ((p_info->assignment_ == nullptr) != (m_info->assignment_ == nullptr))
        break;

      if (p_info->assignment_ && m_info->assignment_) {
        if (p_info->label_->index_ != m_info->label_->index_)
          break;
        --p_pos;
        --m_pos;
        continue;
      }

      if (p_info->refs_ != m_info->refs_)
        break;

      AssignOne(p_info, m_info);
      VLOG(4) << "    Extending assignment seq[-" << p_pos_start - p_pos
              << "] <- " << ToString(p_info) << " := " << ToString(m_info);

      --p_pos;
      --m_pos;
    }

    return p_pos - p_pos_start;
  }

  Node* FindModelNode(Node* node) {
    if (node->prev_ == nullptr)
      return m_root_;

    Node* m_parent = FindModelNode(node->prev_);
    if (m_parent == nullptr) {
      return nullptr;
    }

    ExtendNode(m_parent, m_trace_);

    LabelInfo* p_label = node->in_edge_;
    LabelInfo* m_label = p_label->assignment_;
    if (m_label == nullptr) {
      VLOG(2) << "Expected assigned prefix";
      return nullptr;
    }

    Node::Edges::iterator e = m_parent->edges_.find(m_label);
    if (e == m_parent->edges_.end()) {
      VLOG(3) << "Expected defined edge in parent";
      return nullptr;
    }

    return e->second;
  }

  Node* MakeRootNode(const Trace& trace) {
    Node* node = new Node(nullptr, nullptr);
    all_nodes_.push_back(node);
    for (uint32_t i = 0; i < trace.size(); ++i) {
      ++node->count_;
      node->places_.push_back(i);
    }
    return node;
  }

  void ExtendNode(Node* node, const Trace& trace) {
    // Make sure trie is filled in at this node.
    if (node->Extended())
      return;
    for (size_t i = 0; i < node->places_.size();  ++i) {
      uint32_t index = node->places_.at(i);
      if (index < trace.size()) {
        LabelInfo* item = trace.at(index);
        Node*& slot = node->edges_[item];
        if (slot == nullptr) {
          slot = new Node(item, node);
          all_nodes_.push_back(slot);
          node->edges_in_frequency_order.push_back(slot);
        }
        slot->places_.push_back(index + 1);
        ++slot->count_;
      }
    }
    node->edges_in_frequency_order.sort(OrderNodeByCountDecreasing());
  }

  const Trace& m_trace_;
  const Trace& p_trace_;
  Node* m_root_;
  Node* p_root_;

  NodeQueue worklist_;
  NodeQueue unsolved_;

  std::vector<Node*> all_nodes_;

  DISALLOW_COPY_AND_ASSIGN(AssignmentProblem);
};

class GraphAdjuster : public AdjustmentMethod {
 public:
  GraphAdjuster()
      : prog_(nullptr), model_(nullptr), debug_label_index_gen_(0) {}
  ~GraphAdjuster() = default;

  bool Adjust(const AssemblyProgram& model, AssemblyProgram* program) {
    VLOG(1) << "GraphAdjuster::Adjust";
    prog_ = program;
    model_ = &model;
    debug_label_index_gen_ = 0;
    return Finish();
  }

  bool Finish() {
    prog_->UnassignIndexes();
    CollectTraces(model_, &model_abs32_, &model_rel32_, true);
    CollectTraces(prog_,  &prog_abs32_,  &prog_rel32_,  false);
    Solve(model_abs32_, prog_abs32_);
    Solve(model_rel32_, prog_rel32_);
    prog_->AssignRemainingIndexes();
    return true;
  }

 private:
  void CollectTraces(const AssemblyProgram* program, Trace* abs32, Trace* rel32,
                     bool is_model) {
    for (Label* label : program->abs32_label_annotations())
      ReferenceLabel(abs32, is_model, label);
    for (Label* label : program->rel32_label_annotations())
      ReferenceLabel(rel32, is_model, label);

    // TODO(sra): we could simply append all the labels in index order to
    // incorporate some costing for entropy (bigger deltas) that will be
    // introduced into the label address table by non-monotonic ordering.  This
    // would have some knock-on effects to parts of the algorithm that work on
    // single-occurrence labels.
  }

  void Solve(const Trace& model, const Trace& problem) {
    LinkLabelInfos(model);
    LinkLabelInfos(problem);
    AssignmentProblem a(model, problem);
    a.Solve();
  }

  void LinkLabelInfos(const Trace& trace) {
    typedef std::set<LabelInfo*, OrderLabelInfoByAddressAscending> Ordered;
    Ordered ordered;
    for (Trace::const_iterator p = trace.begin();  p != trace.end();  ++p)
      ordered.insert(*p);
    LabelInfo* prev = nullptr;
    for (Ordered::iterator p = ordered.begin();  p != ordered.end();  ++p) {
      LabelInfo* curr = *p;
      if (prev) prev->next_addr_ = curr;
      curr->prev_addr_ = prev;
      prev = curr;

      if (curr->positions_.size() != curr->refs_)
        NOTREACHED();
    }
  }

  void ReferenceLabel(Trace* trace, bool is_model, Label* label) {
    trace->push_back(
        MakeLabelInfo(label, is_model, static_cast<uint32_t>(trace->size())));
  }

  LabelInfo* MakeLabelInfo(Label* label, bool is_model, uint32_t position) {
    LabelInfo& slot = label_infos_[label];
    if (slot.label_ == nullptr) {
      slot.label_ = label;
      slot.is_model_ = is_model;
      slot.debug_index_ = ++debug_label_index_gen_;
    }
    slot.positions_.push_back(position);
    ++slot.refs_;
    return &slot;
  }

  AssemblyProgram* prog_;         // Program to be adjusted, owned by caller.
  const AssemblyProgram* model_;  // Program to be mimicked, owned by caller.

  Trace model_abs32_;
  Trace model_rel32_;
  Trace prog_abs32_;
  Trace prog_rel32_;

  int debug_label_index_gen_;

  // Note LabelInfo is allocated inside map, so the LabelInfo lifetimes are
  // managed by the map.
  std::map<Label*, LabelInfo> label_infos_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GraphAdjuster);
};


////////////////////////////////////////////////////////////////////////////////

void AdjustmentMethod::Destroy() { delete this; }

AdjustmentMethod* AdjustmentMethod::MakeNullAdjustmentMethod() {
  return new NullAdjustmentMethod();
}

AdjustmentMethod* AdjustmentMethod::MakeTrieAdjustmentMethod() {
  return new GraphAdjuster();
}

Status Adjust(const AssemblyProgram& model, AssemblyProgram* program) {
  AdjustmentMethod* method = AdjustmentMethod::MakeProductionAdjustmentMethod();
  bool ok = method->Adjust(model, program);
  method->Destroy();
  if (ok)
    return C_OK;
  else
    return C_ADJUSTMENT_FAILED;
}

}  // namespace courgette
